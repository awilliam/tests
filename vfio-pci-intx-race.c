#include <errno.h>
#include <libgen.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/eventfd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <linux/ioctl.h>
#include <linux/vfio.h>


void usage(char *name)
{
	printf("usage: %s ssss:bb:dd.f\n", name);
	printf("\tssss: PCI segment, ex. 0000\n");
	printf("\tbb:   PCI bus, ex. 01\n");
	printf("\tdd:   PCI device, ex. 06\n");
	printf("\tf:    PCI function, ex. 0\n");
}

int main(int argc, char **argv)
{
	int seg, bus, slot, func;
	int ret, container, group, device, groupid, intx, unmask;
	char path[50], iommu_group_path[50], *group_name;
	struct stat st;
	ssize_t len;
	struct vfio_group_status group_status = {
		.argsz = sizeof(group_status)
	};
	struct vfio_device_info device_info = {
		.argsz = sizeof(device_info)
	};
	struct vfio_irq_info irq_info = {
		.argsz = sizeof(irq_info),
		.index = VFIO_PCI_INTX_IRQ_INDEX
	};
	struct vfio_irq_set *irq_set;
	int32_t *pfd;

	if (argc != 2) {
		usage(argv[0]);
		return -1;
	}

	/* Boilerplate vfio setup */
	ret = sscanf(argv[1], "%04x:%02x:%02x.%d", &seg, &bus, &slot, &func);
	if (ret != 4) {
		usage(argv[0]);
		return -1;
	}

	container = open("/dev/vfio/vfio", O_RDWR);
	if (container < 0) {
		printf("Failed to open /dev/vfio/vfio, %d (%s)\n",
		       container, strerror(errno));
		return container;
	}

	snprintf(path, sizeof(path),
		 "/sys/bus/pci/devices/%04x:%02x:%02x.%01x/",
		 seg, bus, slot, func);

	ret = stat(path, &st);
	if (ret < 0) {
		printf("No such device\n");
		return  ret;
	}

	strncat(path, "iommu_group", sizeof(path) - strlen(path) - 1);

	len = readlink(path, iommu_group_path, sizeof(iommu_group_path));
	if (len <= 0) {
		printf("No iommu_group for device\n");
		return -1;
	}

	iommu_group_path[len] = 0;
	group_name = basename(iommu_group_path);

	if (sscanf(group_name, "%d", &groupid) != 1) {
		printf("Unknown group\n");
		return -1;
	}

	snprintf(path, sizeof(path), "/dev/vfio/%d", groupid);
	group = open(path, O_RDWR);
	if (group < 0) {
		printf("Failed to open %s, %d (%s)\n",
		       path, group, strerror(errno));
		return group;
	}

	ret = ioctl(group, VFIO_GROUP_GET_STATUS, &group_status);
	if (ret) {
		printf("ioctl(VFIO_GROUP_GET_STATUS) failed\n");
		return ret;
	}

	if (!(group_status.flags & VFIO_GROUP_FLAGS_VIABLE)) {
		printf("Group not viable, are all devices attached to vfio?\n");
		return -1;
	}

	ret = ioctl(group, VFIO_GROUP_SET_CONTAINER, &container);
	if (ret) {
		printf("Failed to set group container\n");
		return ret;
	}

	ret = ioctl(container, VFIO_SET_IOMMU, VFIO_TYPE1_IOMMU);
	if (ret) {
		printf("Failed to set IOMMU\n");
		return ret;
	}

	close(container);

	snprintf(path, sizeof(path), "%04x:%02x:%02x.%d", seg, bus, slot, func);

	device = ioctl(group, VFIO_GROUP_GET_DEVICE_FD, path);
	if (device < 0) {
		printf("Failed to get device %s\n", path);
		return -1;
	}

	close(group);

	if (ioctl(device, VFIO_DEVICE_GET_INFO, &device_info)) {
		printf("Failed to get device info\n");
		return -1;
	}

	if (!(device_info.flags & VFIO_DEVICE_FLAGS_PCI)) {
		printf("Error, not a PCI device\n");
		return -1;
	}

	if (device_info.num_irqs < VFIO_PCI_INTX_IRQ_INDEX + 1) {
		printf("Error, device does not support INTx\n");
		return -1;
	}

	if (ioctl(device, VFIO_DEVICE_GET_IRQ_INFO, &irq_info)) {
		printf("Failed to get IRQ info\n");
		return -1;
	}

	if (irq_info.count != 1 || !(irq_info.flags & VFIO_IRQ_INFO_EVENTFD)) {
		printf("Unexpected IRQ info properties\n");
		return -1;
	}

	irq_set = malloc(sizeof(*irq_set) + sizeof(*pfd));
	if (!irq_set) {
		printf("Failed to malloc irq_set\n");
		return -1;
	}

	irq_set->argsz = sizeof(*irq_set) + sizeof(*pfd);
	irq_set->index = VFIO_PCI_INTX_IRQ_INDEX;
	irq_set->start = 0;
	pfd = (int32_t *)&irq_set->data;

	intx = eventfd(0, EFD_CLOEXEC);
	if (intx < 0) {
		printf("Failed to get intx eventfd\n");
		return -1;
	}

	unmask = eventfd(0, EFD_CLOEXEC);
	if (unmask < 0) {
		printf("Failed to get unmask eventfd\n");
		return -1;
	}

	if (fork()) {

		printf("Enable/disable thread (%d)...\n", getpid());

		while (1) {
			*pfd = intx;
			irq_set->flags = VFIO_IRQ_SET_DATA_EVENTFD | VFIO_IRQ_SET_ACTION_TRIGGER;
			irq_set->count = 1;

			if (ioctl(device, VFIO_DEVICE_SET_IRQS, irq_set))
				printf("INTx enable (%m)\n");

			*pfd = unmask;
			irq_set->flags = VFIO_IRQ_SET_DATA_EVENTFD | VFIO_IRQ_SET_ACTION_UNMASK;

			if (ioctl(device, VFIO_DEVICE_SET_IRQS, irq_set))
				printf("unmask irqfd (%m)\n");

			//printf("+");
			//fflush(stdout);

			irq_set->flags = VFIO_IRQ_SET_DATA_NONE | VFIO_IRQ_SET_ACTION_TRIGGER;
			irq_set->count = 0;

			if (ioctl(device, VFIO_DEVICE_SET_IRQS, irq_set))
				printf("INTx disable (%m)\n");

			//printf("-");
			//fflush(stdout);
		}
	} else if (fork()) {
		uint64_t buf;

		printf("Consumer thread (%d)...\n", getpid());

		close(unmask);

		while (read(intx, &buf, sizeof(buf)) == sizeof(buf)) {
			//printf("!");
			//fflush(stdout);
		}
	} else {
		uint64_t buf = 1;

		printf("Unmask thread (%d)...\n", getpid());

		close(intx);

		while (write(unmask, &buf, sizeof(buf)) == sizeof(buf)) {
			//printf("#");
			//fflush(stdout);
		}
	}

	return 0;
}
