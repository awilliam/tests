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
#include <libgen.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/vfs.h>

#include <linux/ioctl.h>
#include <linux/vfio.h>

#include "utils.h"

void usage(char *name)
{
	printf("usage: %s <ssss:bb:dd.f>\n", name);
}

#define HIGH_MEM (4ul * 1024 * 1024 * 1024)

static void do_map_unmap(int container, int device,
			 struct vfio_region_info *region,
			 unsigned long iova_base,
			 unsigned long dma_size)
{
	struct vfio_iommu_type1_dma_map dma_map = { 0 };
	struct vfio_iommu_type1_dma_unmap dma_unmap = { 0 };
	unsigned long before, after;
	int ret;

	void *map = mmap_align(NULL, (size_t)region->size, PROT_READ, MAP_SHARED, device,
			       (off_t)region->offset, dma_size);
	if (map == MAP_FAILED) {
		printf("mmap failed: %s\n", strerror(errno));
		return;
	}

	dma_map.argsz = sizeof(dma_map);
	dma_map.vaddr = (__u64)map;
	dma_map.size = dma_size;
	dma_map.iova = iova_base;
	dma_map.flags = VFIO_DMA_MAP_FLAG_READ;

	before = now_nsec();
	while (dma_map.iova < iova_base + region->size) {
		ret = ioctl(container, VFIO_IOMMU_MAP_DMA, &dma_map);
		if (ret) {
			printf("VFIO_IOMMU_MAP_DMA failed at 0x%llx: %d (%s)\n",
			       dma_map.iova, ret, strerror(errno));
			goto unmap;
		}
		dma_map.iova += dma_size;
	}
	after = now_nsec();

	printf("\tdma size %9ldK mmapped in %3ld.%03lds\n", dma_size / 1024,
	       (after - before) / NSEC_PER_SEC,
	       ((after - before) % NSEC_PER_SEC) / USEC_PER_SEC);

unmap:
	dma_unmap.argsz = sizeof(dma_unmap);
	dma_unmap.iova = iova_base;
	dma_unmap.size = region->size;
	ret = ioctl(container, VFIO_IOMMU_UNMAP_DMA, &dma_unmap);
	if (ret) {
		printf("VFIO_IOMMU_UNMAP_DMA failed at 0x%llx: %d (%s)\n",
		       dma_unmap.iova, ret, strerror(errno));
	}

	munmap(map, (size_t)region->size);
}

int main(int argc, char **argv)
{
	const char *devname;
	int container, device;
	int i;
	int ret;
	struct vfio_device_info device_info = {	.argsz = sizeof(device_info) };
	struct vfio_region_info region_info = { .argsz = sizeof(region_info) };

	if (argc < 2) {
		usage(argv[0]);
		return -1;
	}

	devname = argv[1];

	if (vfio_device_attach(devname, &container, &device, NULL))
		return -1;

	ret = ioctl(device, VFIO_DEVICE_GET_INFO, &device_info);
	if (ret) {
		printf("VFIO_DEVICE_GET_INFO failed: %d (%s)\n",
		       ret, strerror(errno));
		return -1;
	}

	printf("Device %s supports %d regions, %d irqs\n", devname,
	       device_info.num_regions, device_info.num_irqs);

	for (i = 0; i < device_info.num_regions; i++) {
		printf("Region %d: ", i);
		region_info.index = i;
		ret = ioctl(device, VFIO_DEVICE_GET_REGION_INFO, &region_info);
		if (ret) {
			printf("VFIO_DEVICE_GET_REGION_INFO failed for region %d: %d (%s)\n",
			       region_info.index, ret, strerror(errno));
			continue;
		}

		printf("size 0x%lx, offset 0x%lx, flags 0x%x\n",
		       (unsigned long)region_info.size,
		       (unsigned long)region_info.offset, region_info.flags);

		if (region_info.flags & VFIO_REGION_INFO_FLAG_MMAP) {
			unsigned long dma_sizes[] = {
				2ul * 1024 * 1024,
				1ul * 1024ul * 1024 * 1024,
				region_info.size,
				0
			};

			for (int j = 0; dma_sizes[j]; j++) {
				if (dma_sizes[j] > region_info.size)
					continue;
				do_map_unmap(container, device, &region_info,
					     HIGH_MEM, dma_sizes[j]);
			}
		}
	}

	return 0;
}
