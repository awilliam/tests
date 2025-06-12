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
		unsigned long config_offset;
		char sig[17];
		unsigned size, tmp;

		printf("Region %d: ", i);
		region_info.index = i;
		if (ioctl(device, VFIO_DEVICE_GET_REGION_INFO, &region_info)) {
			printf("Failed to get info\n");
			continue;
		}

		printf("size 0x%lx, offset 0x%lx, flags 0x%x\n",
		       (unsigned long)region_info.size,
		       (unsigned long)region_info.offset, region_info.flags);

		if (i == VFIO_PCI_CONFIG_REGION_INDEX)
			config_offset = region_info.offset;

		if (region_info.flags & VFIO_REGION_INFO_FLAG_CAPS) {
			struct vfio_region_info *region;
			void *buf;
			struct vfio_info_cap_header *header;
			struct vfio_region_info_cap_type *type;

			printf("Found caps flag, realloc %d bytes\n",
				region_info.argsz);


			buf = region = malloc(region_info.argsz);
			if (!region) {
				printf("Can't new buffer\n");
				return -1;
			}

			region->argsz = region_info.argsz;
			region->index = i;

			if (ioctl(device,
			          VFIO_DEVICE_GET_REGION_INFO, region)) {

				printf("Failed to re-get info\n");
				return -1;
			}

			if (region->argsz != region_info.argsz) {
				printf("Size changed!?\n");
				return -1;
			}

			if (!(region->flags & VFIO_REGION_INFO_FLAG_CAPS)) {
				printf("Caps disappeared?!\n");
				return -1;
			}

			printf("First cap @%x\n", region->cap_offset);

			header = buf + region->cap_offset;

			printf("header ID %x version %x next %x\n",
			       header->id, header->version, header->next);

			if (header->id != VFIO_REGION_INFO_CAP_TYPE) {
				printf("Unexpected ID, this code doesn't walk the chain\n");
				return -1;
			}


			type = (struct vfio_region_info_cap_type *)header;

			printf("Type %x, sub-type %x\n",
			       type->type, type->subtype);


			if (pread(device, sig, 16, region->offset) != 16) {
				printf("failed to read signature\n");
				return -1;
			}

			sig[16] = 0;

			printf("IGD opregion signature: %s\n", sig);

			if (pread(device, &size, 4, region->offset + 16) != 4) {
				printf("failed to read size\n");
				return -1;
			}

			printf("IGD opregion size %dKB\n", size);

			if (pread(device, &tmp, 4, config_offset + 0xfc) != 4) {
				printf("failed to read config\n");
				return -1;
			}

			printf("IGD opregion address: %08x\n", tmp);

			tmp = 0;

			pwrite(device, &tmp, 4, config_offset + 0xfc);
			if (pread(device, &tmp, 4, config_offset + 0xfc) != 4) {
				printf("failed to re-read config\n");
				return -1;
			}

			printf("IGD opregion virt address: %08x\n", tmp);

			free(buf);
		}

		region_info.argsz = sizeof(region_info);
	}

	printf("Success\n");
	//printf("Press any key to exit\n");
	//fgetc(stdin);

	return 0;
}
