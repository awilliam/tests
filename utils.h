/*
 * VFIO test suite
 *
 * Copyright (C) 2012-2025, Red Hat Inc.
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 */

#ifndef VFIO_TESTSUITE_UTILS_H
#define VFIO_TESTSUITE_UTILS_H

#include <time.h>

/*
 * Logging
 */
extern int verbose;

int vfio_group_attach(int groupid, int *container_out, int *group_out);
int vfio_device_attach(const char *devname, int *container_out,
		       int *device_out, int *group_out);
int vfio_device_iommufd_getfd(const char *devname);

#define NSEC_PER_SEC 1000000000ul
#define USEC_PER_SEC 1000000ul

static inline unsigned long now_nsec(void)
{
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC_RAW, &ts);
	return NSEC_PER_SEC * (unsigned long) ts.tv_sec + ts.tv_nsec;
}

void *mmap_align(void *addr, size_t length, int prot, int flags,
		 int fd, off_t offset, size_t align);

#endif /* VFIO_TESTSUITE_UTILS_H */
