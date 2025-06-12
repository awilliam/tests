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

#define false 0
#define true 1

int main(int argc, char **argv)
{
       int i, ret, container, group, device, *pfd;
	const char *devname;
	struct vfio_device_info device_info = {	.argsz = sizeof(device_info) };
	struct vfio_region_info region_info = { .argsz = sizeof(region_info) };

	struct vfio_group_status group_status = {
		.argsz = sizeof(group_status)
	};
	struct vfio_pci_hot_reset_info *reset_info;
	struct vfio_pci_dependent_device *devices;
	struct vfio_pci_hot_reset *reset;
	
	if (argc < 2) {
		usage(argv[0]);
		return -1;
	}

	devname = argv[1];

	if (vfio_device_attach(devname, &container, &device, &group))
		return -1;

	reset_info = malloc(sizeof(*reset_info));
	if (!reset_info) {
		printf("Failed to alloc info struct\n");
		return -ENOMEM;
	}

	reset_info->argsz = sizeof(*reset_info);

	ret = ioctl(device, VFIO_DEVICE_GET_PCI_HOT_RESET_INFO, reset_info);
	if (ret && errno == ENODEV) {
		printf("Device does not support hot reset\n");
		return 0;
	}
	if (!ret || errno != ENOSPC) {
		printf("Expected fail/-ENOSPC, got %d/%d\n", ret, -errno);
		return -1;
	}

	printf("Dependent device count: %d\n", reset_info->count);

	reset_info = realloc(reset_info, sizeof(*reset_info) +
			     (reset_info->count * sizeof(*devices)));
	if (!reset_info) {
		printf("Failed to re-alloc info struct\n");
		return -ENOMEM;
	}

	reset_info->argsz = sizeof(*reset_info) +
                             (reset_info->count * sizeof(*devices));
	ret = ioctl(device, VFIO_DEVICE_GET_PCI_HOT_RESET_INFO, reset_info);
	if (ret) {
		printf("Reset Info error\n");
		return ret;
	}

	devices = &reset_info->devices[0];

	for (i = 0; i < reset_info->count; i++)
		printf("%d: %04x:%02x:%02x.%d group %d\n", i,
		       devices[i].segment, devices[i].bus,
		       devices[i].devfn >> 3, devices[i].devfn & 7,
		       devices[i].group_id);

	printf("Attempting reset: ");
	fflush(stdout);

	reset = malloc(sizeof(*reset) + sizeof(*pfd));
	pfd = &reset->group_fds[0];

	*pfd = group;

	reset->argsz = sizeof(*reset) + sizeof(*pfd);
	reset->count = 1;
	reset->flags = 0;

	ret = ioctl(device, VFIO_DEVICE_PCI_HOT_RESET, reset);
	printf("%s\n", ret ? "Failed" : "Pass");

	return ret;
}
