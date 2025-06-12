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
	int i, ret, container, group, device, groupid;
	char path[PATH_MAX];
	int seg, bus, dev, func;

	struct vfio_group_status group_status = {
		.argsz = sizeof(group_status)
	};

	struct vfio_device_info device_info = {
		.argsz = sizeof(device_info)
	};

	struct vfio_region_info region_info = {
		.argsz = sizeof(region_info)
	};

	if (argc < 3) {
		usage(argv[0]);
		return -1;
	}

	ret = sscanf(argv[1], "%d", &groupid);
	if (ret != 1) {
		usage(argv[0]);
		return -1;
	}

	ret = sscanf(argv[2], "%04x:%02x:%02x.%d", &seg, &bus, &dev, &func);
	if (ret != 4) {
		usage(argv[0]);
		return -1;
	}

	printf("Using PCI device %04x:%02x:%02x.%d in group %d\n",
               seg, bus, dev, func, groupid);

	container = open("/dev/vfio/vfio", O_RDWR);
	if (container < 0) {
		printf("Failed to open /dev/vfio/vfio, %d (%s)\n",
		       container, strerror(errno));
		return container;
	}

	snprintf(path, sizeof(path), "/dev/vfio/noiommu-%d", groupid);
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

	printf("pre-SET_CONTAINER:\n");
	printf("VFIO_CHECK_EXTENSION VFIO_TYPE1_IOMMU: %sPresent\n",
	       ioctl(container, VFIO_CHECK_EXTENSION, VFIO_TYPE1_IOMMU) ?
	       "" : "Not ");
	printf("VFIO_CHECK_EXTENSION VFIO_NOIOMMU_IOMMU: %sPresent\n",
	       ioctl(container, VFIO_CHECK_EXTENSION, VFIO_NOIOMMU_IOMMU) ?
	       "" : "Not ");
	
	ret = ioctl(group, VFIO_GROUP_SET_CONTAINER, &container);
	if (ret) {
		printf("Failed to set group container\n");
		return ret;
	}

	printf("post-SET_CONTAINER:\n");
	printf("VFIO_CHECK_EXTENSION VFIO_TYPE1_IOMMU: %sPresent\n",
	       ioctl(container, VFIO_CHECK_EXTENSION, VFIO_TYPE1_IOMMU) ?
	       "" : "Not ");
	printf("VFIO_CHECK_EXTENSION VFIO_NOIOMMU_IOMMU: %sPresent\n",
	       ioctl(container, VFIO_CHECK_EXTENSION, VFIO_NOIOMMU_IOMMU) ?
	       "" : "Not ");
	
	ret = ioctl(container, VFIO_SET_IOMMU, VFIO_TYPE1_IOMMU);
	if (!ret) {
		printf("ERROR, was able to use type1 IOMMU with no-iommu\n");
		return -1;
	}

	ret = ioctl(container, VFIO_SET_IOMMU, VFIO_NOIOMMU_IOMMU);
	if (ret) {
		printf("Failed to set IOMMU\n");
		return ret;
	}

	snprintf(path, sizeof(path), "%04x:%02x:%02x.%d", seg, bus, dev, func);

	device = ioctl(group, VFIO_GROUP_GET_DEVICE_FD, path);
	if (device < 0) {
		printf("Failed to get device %s\n", path);
		return -1;
	}

	if (ioctl(device, VFIO_DEVICE_GET_INFO, &device_info)) {
		printf("Failed to get device info\n");
		return -1;
	}

	printf("Device supports %d regions, %d irqs\n",
	       device_info.num_regions, device_info.num_irqs);

	for (i = 0; i < device_info.num_regions; i++) {
		printf("Region %d: ", i);
		region_info.index = i;
		if (ioctl(device, VFIO_DEVICE_GET_REGION_INFO, &region_info)) {
			printf("Failed to get info\n");
			continue;
		}

		printf("size 0x%lx, offset 0x%lx, flags 0x%x\n",
		       (unsigned long)region_info.size,
		       (unsigned long)region_info.offset, region_info.flags);
		if (0 && region_info.flags & VFIO_REGION_INFO_FLAG_MMAP) {
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
	}

	printf("Success\n");
	printf("Press any key to exit\n");
	fgetc(stdin);

	return 0;
}
