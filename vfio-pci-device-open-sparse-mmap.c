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
		struct vfio_region_info *info;

		info = &region_info;
		printf("Region %d: ", i);
		region_info.index = i;
		if (ioctl(device, VFIO_DEVICE_GET_REGION_INFO, info)) {
			printf("Failed to get info\n");
			continue;
		}

		if (info->flags & VFIO_REGION_INFO_FLAG_CAPS) {
			info = calloc(info->argsz, 1);
			if (!info) {
				printf("Failed to alloc larger info\n");
				info = &region_info;
			} else {
				memcpy(info, &region_info, sizeof(region_info));
				if (ioctl(device,
					  VFIO_DEVICE_GET_REGION_INFO, info)) {
					printf("Failed to re-get info\n");
					continue;
				}
			}
		}

		printf("size 0x%lx, offset 0x%lx, flags 0x%x\n",
		       (unsigned long)info->size,
		       (unsigned long)info->offset, info->flags);
		if (0 && info->flags & VFIO_REGION_INFO_FLAG_MMAP) {
			void *map = mmap(NULL, (size_t)region_info.size,
					 PROT_READ, MAP_SHARED, device,
					 (off_t)region_info.offset);
			if (map == MAP_FAILED) {
				printf("mmap failed\n");
				continue;
			}

			printf("[");
			fwrite(map, 1, region_info.size > 16 ? 16 :
						region_info.size, stdout);
			printf("]\n");
			munmap(map, (size_t)region_info.size);
		}

		if (info->flags & VFIO_REGION_INFO_FLAG_CAPS) {
			struct vfio_info_cap_header *header;

			header = (void *)info + info->cap_offset;

next:
			printf("\tCapability @%d: ID %d, version %d, next %d\n",
			       info->cap_offset, header->id, header->version,
			       header->next);

			if (header->id == VFIO_REGION_INFO_CAP_SPARSE_MMAP) {
				struct vfio_region_info_cap_sparse_mmap *sparse;
				int i;

				sparse = (void *)header;

				printf("\t\tsparse mmap cap, nr_areas %d\n",
				       sparse->nr_areas);

				for (i = 0; i < sparse->nr_areas; i++)
					printf("\t\t\t%d: %llx-%llx\n", i,
					       sparse->areas[i].offset,
					       sparse->areas[i].offset +
					       sparse->areas[i].size);
			}

			if (header->next) {
				header = (void *)info + header->next;
				goto next;
			}
		}

		if (info != &region_info)
			free(info);
	}

	printf("Success\n");
	//printf("Press any key to exit\n");
	//fgetc(stdin);

	return 0;
}
