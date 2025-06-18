/*
 * VFIO test suite
 *
 * Copyright (C) 2012-2025, Red Hat Inc.
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 */

#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>

#include <linux/vfio.h>

#include "utils.h"

void usage(char *name)
{
	printf("usage: %s <ssss:bb:dd.f>\n", name);
}

static bool vfio_device_dma_logging_supported(int fd)
{
	uint64_t buf[DIV_ROUND_UP(sizeof(struct vfio_device_feature),
				  sizeof(uint64_t))] = {};
	struct vfio_device_feature *feature = (struct vfio_device_feature *)buf;

	feature->argsz = sizeof(buf);
	feature->flags = VFIO_DEVICE_FEATURE_PROBE |
		VFIO_DEVICE_FEATURE_DMA_LOGGING_START;

	return !ioctl(fd, VFIO_DEVICE_FEATURE, feature);
}

static int vfio_device_query_migration_flags(int fd, uint64_t *mig_flags)
{
	uint64_t buf[DIV_ROUND_UP(sizeof(struct vfio_device_feature) +
				  sizeof(struct vfio_device_feature_migration),
				  sizeof(uint64_t))] = {};
	struct vfio_device_feature *feature = (struct vfio_device_feature *)buf;
	struct vfio_device_feature_migration *mig =
		(struct vfio_device_feature_migration *)feature->data;
    
	feature->argsz = sizeof(buf);
	feature->flags = VFIO_DEVICE_FEATURE_GET | VFIO_DEVICE_FEATURE_MIGRATION;
    
	if (ioctl(fd, VFIO_DEVICE_FEATURE, feature)) {
		if (errno != ENOTTY) {
			printf("Failed to query migration features: %d %s\n",
			       errno, strerror(errno));
			return -1;
		}
	}
	*mig_flags = mig->flags;
	return 0;
}

int main(int argc, char **argv)
{
	const char *devname;
	int device;
	uint64_t mig_flags;
	
	if (argc < 2) {
		usage(argv[0]);
		return -1;
	}

	devname = argv[1];
	
	if (vfio_device_attach(devname, NULL, &device, NULL))
		return -1;
	
	if (vfio_device_query_migration_flags(device, &mig_flags))
		return -1;

	if (mig_flags) {
		printf("Migration features:\n");
		if (mig_flags & VFIO_MIGRATION_STOP_COPY)
			printf("\tstop-copy\n");
		if (mig_flags & VFIO_MIGRATION_P2P)
			printf("\tp2p\n");
		if (mig_flags & VFIO_MIGRATION_PRE_COPY)
			printf("\tpre-copy\n");
		if (vfio_device_dma_logging_supported(device))
			printf("\tdirty-tracking\n");
	} else {
		printf("No migration support\n");
	}
		

	printf("Success\n");
	return 0;
}
