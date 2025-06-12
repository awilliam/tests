#include <errno.h>
#include <libgen.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <linux/ioctl.h>
#include <linux/vfio.h>

#define MAP_SIZE (1UL * 1024 * 1024 * 1024)
#define MAP_CHUNK (4 * 1024)
#define REALLOC_INTERVAL 30

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
	int ret, container, group, groupid;
	char path[50], iommu_group_path[50], *group_name;
	struct stat st;
	ssize_t len;
	unsigned long i, count;
	void **maps;
	struct vfio_group_status group_status = {
		.argsz = sizeof(group_status)
	};
	struct vfio_iommu_type1_dma_map dma_map = {
		.argsz = sizeof(dma_map)
	};
	struct vfio_iommu_type1_dma_unmap dma_unmap = {
		.argsz = sizeof(dma_unmap)
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

	ret = ioctl(container, VFIO_SET_IOMMU, VFIO_TYPE1_IOMMU);
	if (ret) {
		printf("Failed to set IOMMU\n");
		return ret;
	}

	/* Test code */
	dma_map.flags = VFIO_DMA_MAP_FLAG_READ | VFIO_DMA_MAP_FLAG_WRITE;
	dma_map.size = MAP_CHUNK;
	dma_unmap.size = MAP_SIZE;
	dma_unmap.iova = 0;

	/* Track our mmaps for re-use */
	maps = malloc(sizeof(void *) * (MAP_SIZE/dma_map.size));
	if (!maps) {
		printf("Failed to allocate map (%s)\n", strerror(errno));
		return -1;
	}

	memset(maps, 0, sizeof(void *) * (MAP_SIZE/dma_map.size));

	for (count = 0;; count++) {

		/* Every REALLOC_INTERVAL, dump our mappings to give THP something to collapse */
		if (count % REALLOC_INTERVAL == 0) {
			for (i = 0; i < MAP_SIZE/dma_map.size; i++) {
				if (maps[i]) {
					munmap(maps[i], dma_map.size);
					maps[i] = NULL;
				}
			}
			if (count) {
				printf("\t%ld\n", count);
				//return 0;
			}
			printf("|");
			fflush(stdout);
		}

		/* Map MAP_CHUNK at a time, each chunk is pinned on map, so THP can't do anything until unmap */
		for (i = dma_map.iova = 0; i < MAP_SIZE/dma_map.size; i++, dma_map.iova += dma_map.size) {
			if (!maps[i]) {
				maps[i] = mmap(NULL, dma_map.size,
						PROT_READ | PROT_WRITE,
						MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
				if (maps[i] == MAP_FAILED) {
					printf("Failed to mmap memory (%s)\n", strerror(errno));
					return -1;
				}
			}

			ret = madvise(maps[i], dma_map.size, MADV_HUGEPAGE);
			if (ret) {
				printf("Madvise failed (%s)\n", strerror(errno));
			}

			dma_map.vaddr = (unsigned long)maps[i];

			ret = ioctl(container, VFIO_IOMMU_MAP_DMA, &dma_map);
			if (ret) {
				printf("Failed to map memory (%s)\n",
					strerror(errno));
				return ret;
			}
		}

		printf("+");
		fflush(stdout);

		/* Unmap everything at once */
		ret = ioctl(container, VFIO_IOMMU_UNMAP_DMA, &dma_unmap);
		if (ret) {
			printf("Failed to unmap memory (%s)\n", strerror(errno));
			return ret;
		}

		printf("-");
		fflush(stdout);
	}

	return 0;
}
