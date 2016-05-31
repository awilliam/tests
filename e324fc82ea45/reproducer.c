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

	//snprintf(path, sizeof(path), "%04x:%02x:%02x.%d", seg, bus, slot, func);
	snprintf(path, sizeof(path), "THIS-IS-NOT-THE-RIGHT-NAME");

        for (;;) {
		device = ioctl(group, VFIO_GROUP_GET_DEVICE_FD, path);
		if (device >= 0) {
			printf("Got a device named \"%s\"???\n", path);
			break;
		}
	}

	close(group);
	close(device);

	return -1;
}
