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
#include <pthread.h>

#include <linux/ioctl.h>
#include <linux/vfio.h>

#include "utils.h"

void usage(char *name)
{
	printf("usage: %s <ssss:bb:dd.f>\n", name);
}

static int go;

static void *thread_func(void *map)
{
	volatile unsigned long *val = map;

	do {} while (!go);

	(void)*val;
	return NULL;
}

static void do_race(int device, struct vfio_region_info *region, size_t pagesz)
{
	int i;

	for (i = 0; i < 100000;) {
		pthread_t thread1, thread2;
		void *map;
		
		map = mmap_align(NULL, pagesz, PROT_READ, MAP_SHARED,
						 device, (off_t)region->offset, pagesz);
		if (map == MAP_FAILED) {
			printf("mmap failed: %s\n", strerror(errno));
			return;
		}

		madvise(map, pagesz, MADV_HUGEPAGE);

		go = 0;
		pthread_create(&thread1, NULL, thread_func, map + pagesz - getpagesize());
		pthread_create(&thread2, NULL, thread_func, map);
		go = 1;
		pthread_join(thread1, NULL);
		pthread_join(thread2, NULL);

		munmap(map, pagesz);
		if (!(++i % 10000)) {
			printf(".");
			fflush(stdout);
		}
	}
	printf(" [DONE]\n");
}

int main(int argc, char **argv)
{
	const char *devname;
	int container, device;
	int region, ret;
	struct vfio_device_info device_info = {	.argsz = sizeof(device_info) };
	struct vfio_region_info region_info = { .argsz = sizeof(region_info) };
	size_t *pgsize, pgsizes[] = {
		2ul * 1024 * 1024,
		/* Only PMD generates this issue */
		1ul * 1024ul * 1024 * 1024,
		0
	};

	if (argc < 2) {
		usage(argv[0]);
		return -EINVAL;
	}

	devname = argv[1];

	if (vfio_device_attach(devname, &container, &device))
		return -1;

	ret = ioctl(device, VFIO_DEVICE_GET_INFO, &device_info);
	if (ret) {
		printf("VFIO_DEVICE_GET_INFO failed: %d (%s)\n",
		       ret, strerror(errno));
		return errno;
	}

	if (!(device_info.flags & VFIO_DEVICE_FLAGS_PCI) ||
		device_info.num_regions < VFIO_PCI_BAR5_REGION_INDEX) {
		printf("Invalid vfio-pci device\n");
		return -ENODEV;
	}

	printf("Running tests, if progress dots stop or system generates errors, the test has failed\n");

	for (pgsize = &pgsizes[0]; *pgsize; pgsize++) {
		for (region = 0; region < VFIO_PCI_ROM_REGION_INDEX; region++) {
			region_info.index = region;
			ret = ioctl(device, VFIO_DEVICE_GET_REGION_INFO, &region_info);
			if (ret) {
				printf("VFIO_DEVICE_GET_REGION_INFO failed for region %d: %d (%s)\n",
					   region_info.index, ret, strerror(errno));
				return errno;
			}

			if (!(region_info.flags & VFIO_REGION_INFO_FLAG_MMAP) ||
				region_info.size < *pgsize)
				continue;
	
			printf("Using BAR%d (size %ldMB) for %ldMB page size test\n", region,
					(unsigned long)region_info.size >> 20, *pgsize >> 20);

			do_race(device, &region_info, *pgsize);
			break;
		}

		if (region == VFIO_PCI_ROM_REGION_INDEX) {
			printf("No BAR found for %ldMB test\n", *pgsize >> 20);
			return -ENODEV;
		}
	}

	printf("Check dmesg, if there are any VM_FAULT_OOM messages, the test has failed\n");

	return 0;
}
