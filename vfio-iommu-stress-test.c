/*
 * VFIO test suite
 *
 * Copyright (C) 2012-2025, Red Hat Inc.
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 */

#include <errno.h>
#include <libgen.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <linux/ioctl.h>
#include <linux/vfio.h>

#include "utils.h"

#define MAP_SIZE (1UL * 1024 * 1024 * 1024)
#define MAP_MAX 1024
#define DMA_CHUNK (2UL * 1024 * 1024)

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
	const char *devname;
	int container;
 	unsigned long i, j, vaddr;
	int ret;
	struct vfio_group_status group_status = {
		.argsz = sizeof(group_status)
	};
	struct vfio_iommu_type1_dma_map dma_map = {
		.argsz = sizeof(dma_map)
	};
	struct vfio_iommu_type1_dma_unmap dma_unmap = {
		.argsz = sizeof(dma_unmap)
	};

	if (argc < 2) {
		usage(argv[0]);
		return -1;
	}

	devname = argv[1];

	if (vfio_device_attach(devname, &container, NULL, NULL))
		return -1;

	vaddr = (unsigned long)mmap(0, MAP_SIZE, PROT_READ | PROT_WRITE,
				    MAP_PRIVATE | MAP_ANONYMOUS, 0, 0);
	if (!vaddr) {
		printf("Failed to allocate memory\n");
		return -1;
	}
	printf("%lx\n", vaddr);

	dma_map.flags = VFIO_DMA_MAP_FLAG_READ | VFIO_DMA_MAP_FLAG_WRITE;

	printf("Mapping:   0%%");
	fflush(stdout);
	for (i = 0; i < MAP_MAX; i++) {
		dma_map.size = DMA_CHUNK;

		if (!(i % 3))
			continue;

		for (j = 0; j < MAP_SIZE / DMA_CHUNK; j += 4) {
			dma_map.iova = (i * MAP_SIZE) + (j * DMA_CHUNK);
			dma_map.vaddr = vaddr + (j * DMA_CHUNK);

			ret = ioctl(container, VFIO_IOMMU_MAP_DMA, &dma_map);
			if (ret) {
				printf("Failed to map memory %ld/%ld (%s)\n",
				       i, j, strerror(errno));
				return ret;
			}
		}

#if 1
		for (j = 1; j < MAP_SIZE / DMA_CHUNK; j += 4) {
			dma_map.iova = (i * MAP_SIZE) + (j * DMA_CHUNK);
			dma_map.vaddr = vaddr + (j * DMA_CHUNK);

			ret = ioctl(container, VFIO_IOMMU_MAP_DMA, &dma_map);
			if (ret) {
				printf("Failed to map memory %ld/%ld (%s)\n",
				       i, j, strerror(errno));
				return ret;
			}
		}

		for (j = 3; j < MAP_SIZE / DMA_CHUNK; j += 4) {
			dma_map.iova = (i * MAP_SIZE) + (j * DMA_CHUNK);
			dma_map.vaddr = vaddr + (j * DMA_CHUNK);

			ret = ioctl(container, VFIO_IOMMU_MAP_DMA, &dma_map);
			if (ret) {
				printf("Failed to map memory %ld/%ld (%s)\n",
				       i, j, strerror(errno));
				return ret;
			}
		}

		for (j = 2; j < MAP_SIZE / DMA_CHUNK; j += 4) {
			dma_map.iova = (i * MAP_SIZE) + (j * DMA_CHUNK);
			dma_map.vaddr = vaddr + (j * DMA_CHUNK);

			ret = ioctl(container, VFIO_IOMMU_MAP_DMA, &dma_map);
			if (ret) {
				printf("Failed to map memory %ld/%ld (%s)\n",
				       i, j, strerror(errno));
				return ret;
			}
		}
#endif

		if (((i + 1) * 100)/MAP_MAX != (i * 100)/MAP_MAX) {
			printf("\b\b\b\b%3ld%%", (i * 100)/MAP_MAX);
			fflush(stdout);
		}
	}
	printf("\b\b\b\b100%%\n");

	printf("Unmapping:   0%%");
	fflush(stdout);
	for (i = 0; i < MAP_MAX; i++) {
		dma_unmap.size = DMA_CHUNK;

		if (!(i % 3))
			continue;

		for (j = 0; j < MAP_SIZE / DMA_CHUNK / 2; j += 2) {
			dma_unmap.iova = (i * MAP_SIZE) + (j * DMA_CHUNK);

			ret = ioctl(container,
				    VFIO_IOMMU_UNMAP_DMA, &dma_unmap);
			if (ret) {
				printf("Failed to unmap memory %ld/%ld (%s)\n",
				       i, j, strerror(errno));
				return ret;
			}
		}

#if 1
		for (j = (MAP_SIZE / DMA_CHUNK) - 1;
		     j > MAP_SIZE / DMA_CHUNK / 2; j -= 2) {
			dma_unmap.iova = (i * MAP_SIZE) + (j * DMA_CHUNK);

			ret = ioctl(container,
				    VFIO_IOMMU_UNMAP_DMA, &dma_unmap);
			if (ret) {
				printf("Failed to unmap memory %ld/%ld (%s)\n",
				       i, j, strerror(errno));
				return ret;
			}
		}
#endif

		if (((i + 1) * 100)/MAP_MAX != (i * 100)/MAP_MAX) {
			printf("\b\b\b\b%3ld%%", (i * 100)/MAP_MAX);
			fflush(stdout);
		}
	}
	printf("\b\b\b\b100%%\n");

	return 0;
}
