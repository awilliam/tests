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
#include <time.h>

#include <linux/ioctl.h>
#include <linux/vfio.h>

#include "utils.h"

#define LOOPS 1000000

void usage(char *name)
{
	printf("usage: %s <ssss:bb:dd.f>\n", name);
}

int main(int argc, char **argv)
{
	const char *devname;
	int container, device;
	int i, j, ret, min_align = __builtin_ctzll(getpagesize());
	unsigned long mask;
	void *map;
	struct vfio_device_info device_info = {	.argsz = sizeof(device_info) };
	struct vfio_region_info region_info = { .argsz = sizeof(region_info) };

	if (argc < 2) {
		usage(argv[0]);
		return -1;
	}

	devname = argv[1];

	if (vfio_device_attach(devname, &container, &device))
		return -1;

	ret = ioctl(device, VFIO_DEVICE_GET_INFO, &device_info);
	if (ret) {
		printf("VFIO_DEVICE_GET_INFO failed: %d (%s)\n",
		       ret, strerror(errno));
		return -1;
	}

	printf("Device %s supports %d regions, %d irqs\n", devname,
	       device_info.num_regions, device_info.num_irqs);

	for (i = 0; i < VFIO_PCI_ROM_REGION_INDEX; i++) {
		region_info.index = i;
		ret = ioctl(device, VFIO_DEVICE_GET_REGION_INFO, &region_info);
		if (ret) {
			printf("VFIO_DEVICE_GET_REGION_INFO failed for region %d: %d (%s)\n",
			       region_info.index, ret, strerror(errno));
			continue;
		}

		if (!region_info.size)
			continue;

		printf("[BAR%d]: size 0x%lx, order %d, offset 0x%lx, flags 0x%x\n", i,
		       (unsigned long)region_info.size, __builtin_ctzll((unsigned long long)region_info.size),
		       (unsigned long)region_info.offset, region_info.flags);

		if (region_info.size < 4096 || !(region_info.flags & VFIO_REGION_INFO_FLAG_MMAP)) {
			printf("Skipped\n");
			continue;
		}

		if (region_info.size < (2ULL << 20))
			min_align = 12;
		else if (region_info.size < (1ULL << 30))
			min_align = 21;
		else
			min_align = 30;

		mask = 0;

		printf("Testing BAR%d, require at least %d bit alignment\n", i, min_align);
		for (j = 0; j < LOOPS; j++) {
			map = mmap(NULL, (size_t)region_info.size, PROT_READ | PROT_WRITE, MAP_SHARED,
		   			device, (off_t)region_info.offset);
			if (map == MAP_FAILED) {
				printf("mmap failed: %s\n", strerror(errno));
				return - 1;
			}
			munmap(map, (size_t)region_info.size);
			if (__builtin_ctzll((unsigned long long)map) < min_align) {
				printf("Failed minimum alignment, got order %d want %d (%p)\n",
					__builtin_ctzll((unsigned long long)map), min_align, map);
				return -1;
			}
			mask |= (unsigned long)map;
		}

		printf("[PASS] Minimum alignment %d\n", __builtin_ctzll(mask));

		printf("Testing random offset\n");
		for (j = 0; j < LOOPS; j++) {
			int req_align;
			unsigned long pgs = region_info.size >> 12;
			loff_t offset;
			size_t size;

			if (pgs <= 1)
				break;

			offset = (random() % pgs) * 4096;
			size = region_info.size - offset;

			req_align = __builtin_ctzll(offset | size);
			if (req_align > 30)
				req_align = 30;
			else if (req_align > 21)
				req_align = 21;
			else
				req_align = 12;

			map = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED,
		   			device, (off_t)region_info.offset + offset);
			if (map == MAP_FAILED) {
				printf("mmap failed: offset %lx, %s\n", offset, strerror(errno));
				return - 1;
			}
			munmap(map, size);
			if (__builtin_ctzll((unsigned long long)map) < req_align) {
				printf("Failed random offset, got order %d want %d offset %lx (%p)\n",
					__builtin_ctzll((unsigned long long)map), req_align, offset, map);
				return -1;
			}
		}
		printf("[PASS] Random offset\n");

		printf("Testing random size\n");
		for (j = 0; j < LOOPS; j++) {
			int req_align;
			unsigned long pgs = region_info.size >> 12;
			size_t unmapped, size;

			if (pgs <= 1)
				break;

			unmapped = (random() % pgs) * 4096;

			size = region_info.size - unmapped;
			if (!size)
				continue;

			if (size < (1ULL << 21))
				req_align = 12;
			else if (size < (1ULL << 30))
				req_align = 21;
			else
				req_align = 30;

			map = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED,
		   			device, (off_t)region_info.offset);
			if (map == MAP_FAILED) {
				printf("mmap failed: size %lx, %s\n", size, strerror(errno));
				return - 1;
			}
			munmap(map, size);
			if (__builtin_ctzll((unsigned long long)map) < req_align) {
				printf("Failed random size, got order %d want %d size %lx (%p)\n",
					__builtin_ctzll((unsigned long long)map), req_align, size, map);
			}
		}
		printf("[PASS] Random size\n");
	}

	return 0;
}
