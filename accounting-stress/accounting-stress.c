#define _GNU_SOURCE
#include <errno.h>
#include <libgen.h>
#include <fcntl.h>
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/capability.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <linux/ioctl.h>
#include <linux/vfio.h>

#define MAP_SIZE (4 * 1024)
#define MLOCK_SIZE (4 * 1024)
#define STACK_SIZE (1024 * 1024)

static int stop = 0;

static int mlock_loop(void *buf)
{
	printf("Mlock thread starting\n");

	while (!stop) {
		if (mlock(buf, MLOCK_SIZE)) {
			printf("mlock failed: %m\n");
			continue;
		}
		while (munlock(buf, MLOCK_SIZE))
			printf("munlock failed: %m\n");
	}

	return 0;
}

static void usage(char *name)
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
	int ret, container, group, groupid;
	char path[50], iommu_group_path[50], *group_name;
	struct stat st;
	ssize_t len;
	void *map_buf, *mlock_buf, *stack;
	pid_t pid;
	long i = 0;

	struct vfio_group_status group_status = {
		.argsz = sizeof(group_status)
	};
	struct vfio_iommu_type1_dma_map dma_map = {
		.argsz = sizeof(dma_map),
		.flags = VFIO_DMA_MAP_FLAG_READ | VFIO_DMA_MAP_FLAG_WRITE,
		.size = MAP_SIZE,
		.iova = 0,
	};
	struct vfio_iommu_type1_dma_unmap dma_unmap = {
		.argsz = sizeof(dma_unmap),
		.size = MAP_SIZE,
		.iova = 0,
	};

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

	ret = ioctl(container, VFIO_SET_IOMMU, VFIO_TYPE1v2_IOMMU);
	if (ret) {
		printf("Failed to set IOMMU\n");
		return ret;
	}

	printf("vfio initialized\n");

	map_buf = mmap(NULL, MAP_SIZE, PROT_READ | PROT_WRITE,
		       MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	if (map_buf == MAP_FAILED) {
		printf("Failed to mmap DMA mapping buffer\n");
		return -1;
	}

	dma_map.vaddr = (unsigned long)map_buf;

	printf("DMA buffer allocated\n");
	
	mlock_buf = mmap(NULL, MLOCK_SIZE, PROT_READ | PROT_WRITE,
			 MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	if (mlock_buf == MAP_FAILED) {
		printf("Failed to mmap mlock buffer\n");
		return -1;
	}

	printf("mlock buffer allocated\n");

	stack = malloc(STACK_SIZE);
	if (!stack) {
		printf("Failed to alloc thread stack\n");
		return -1;
	}

	printf("thread stack allocated\n");

	pid = clone(mlock_loop,
		    stack + STACK_SIZE, CLONE_VM | SIGCHLD, mlock_buf);
	if (pid == -1) {
		printf("Failed to clone thread\n");
		return -1;
	}

	printf("Main thread commencing DMA mapping loop\n");

	while (1) {
		if (ioctl(container, VFIO_IOMMU_MAP_DMA, &dma_map)) {
			printf("Failed to map memory (%m)\n");
			break;
		}

		if (ioctl(container, VFIO_IOMMU_UNMAP_DMA, &dma_unmap)) {
			printf("Failed to unmap memory (%m)\n");
			break;
		}
		i++;
		if (!(i % 10000)) {
			printf(".");
			fflush(stdout);
		}
	}

	printf("Iteration count: %ld\n", i);
	stop = 1;
	waitpid(pid, NULL, 0);

	return 0;
}
