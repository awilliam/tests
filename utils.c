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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <libgen.h>
#include <limits.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <dirent.h>

#include <linux/vfio.h>

int verbose;

int vfio_device_iommufd_getfd(const char *devname)
{
	int  domain, bus, dev, func;
	char path[PATH_MAX];
	char *vfio_path = NULL;
	DIR *dir = NULL;
	struct dirent *dent;
	int ret;

	ret = sscanf(devname, "%04x:%02x:%02x.%d", &domain, &bus, &dev, &func);
	if (ret != 4) {
		printf("Invalid PCI device identifier \"%s\"\n", devname);
		return -1;
	}

	snprintf(path, sizeof(path),
		 "/sys/bus/pci/devices/%04x:%02x:%02x.%01x/vfio-dev",
		 domain, bus, dev, func);

	dir = opendir(path);
	if (!dir) {
		printf("couldn't open directory %s", path);
		return -1;
	}

	while ((dent = readdir(dir))) {
		if (!strncmp(dent->d_name, "vfio", 4)) {
			vfio_path = dent->d_name;
			break;
		}
	}
	closedir(dir);

	if (!vfio_path) {
		printf("failed to find vfio-dev/vfioX");
		return -1;
	}

	snprintf(path, sizeof(path), "/dev/vfio/devices/%s", vfio_path);
	ret = open(path, O_RDWR);
	if (ret < 0) {
		printf("Failed to open %s, %d (%s)\n",
		       path, ret, strerror(errno));
	}

	return ret;
}

static int vfio_device_get_groupid(const char *devname)
{
	int  domain, bus, dev, func;
	char group_path[PATH_MAX];
	char tmp[PATH_MAX];
	char *group_name = NULL;
	int ret, groupid;
	ssize_t len;

	ret = sscanf(devname, "%04x:%02x:%02x.%d", &domain, &bus, &dev, &func);
	if (ret != 4) {
		printf("Invalid PCI device identifier \"%s\"\n", devname);
		return -1;
	}

	snprintf(tmp, sizeof(tmp),
		 "/sys/bus/pci/devices/%04x:%02x:%02x.%01x/iommu_group",
		 domain, bus, dev, func);

	len = readlink(tmp, group_path, sizeof(group_path));
	if (len <= 0 || len >= sizeof(group_path)) {
		printf("%s: no iommu_group found\n", devname);
		return -1;
	}

	group_path[len] = 0;

	group_name = basename(group_path);
	if (sscanf(group_name, "%d", &groupid) != 1) {
		printf("failed to read %s", group_path);
		return -1;
	}

	printf("Using device %04x:%02x:%02x.%d in IOMMU group %d\n",
               domain, bus, dev, func, groupid);
	return groupid;
}

static int vfio_group_open(int groupid, bool noiommu)
{
	int fd;
	char path[PATH_MAX];
	int ret;
	struct vfio_group_status status = { .argsz = sizeof(status) };

	snprintf(path, sizeof(path), "/dev/vfio/%s%d",
		 noiommu ? "noiommu-" : "", groupid);
	fd = open(path, O_RDWR);
	if (fd < 0) {
		printf("Failed to open %s, %d (%s)\n",
		       path, fd, strerror(errno));
		return -1;
	}

	ret = ioctl(fd, VFIO_GROUP_GET_STATUS, &status);
	if (ret) {
		printf("failed to get group %d status: %d (%s)\n",
		       groupid, ret, strerror(errno));
		return -1;
	}

	if (!(status.flags & VFIO_GROUP_FLAGS_VIABLE)) {
		printf("group %d is not viable", groupid);
		return -1;
	}

	return fd;
}

static int vfio_group_set_container(int group, int container, int iommu_type)
{
	int ret;
	bool noiommu = VFIO_NOIOMMU_IOMMU == iommu_type;

	struct vfio_group_status group_status = {
		.argsz = sizeof(group_status)
	};

	ret = ioctl(group, VFIO_GROUP_GET_STATUS, &group_status);
	if (ret) {
		printf("ioctl(VFIO_GROUP_GET_STATUS) failed: %d (%s)\n",
		       ret, strerror(errno));
		return ret;
	}

	if (!(group_status.flags & VFIO_GROUP_FLAGS_VIABLE)) {
		printf("Group not viable, are all devices attached to vfio?\n");
		return -1;
	}

	if (verbose) {
		printf("pre-SET_CONTAINER:\n");
		printf("VFIO_CHECK_EXTENSION VFIO_TYPE1_IOMMU: %sPresent\n",
		       ioctl(container, VFIO_CHECK_EXTENSION, VFIO_TYPE1_IOMMU) ?
		       "" : "Not ");
		printf("VFIO_CHECK_EXTENSION VFIO_NOIOMMU_IOMMU: %sPresent\n",
		       ioctl(container, VFIO_CHECK_EXTENSION, VFIO_NOIOMMU_IOMMU) ?
		       "" : "Not ");
	}

	ret = ioctl(group, VFIO_GROUP_SET_CONTAINER, &container);
	if (ret) {
		printf("Failed to set group container: %d (%s)\n", ret, strerror(errno));
		return ret;
	}

	if (verbose) {
		printf("post-SET_CONTAINER:\n");
		printf("VFIO_CHECK_EXTENSION VFIO_TYPE1_IOMMU: %sPresent\n",
		       ioctl(container, VFIO_CHECK_EXTENSION, VFIO_TYPE1_IOMMU) ?
		       "" : "Not ");
		printf("VFIO_CHECK_EXTENSION VFIO_NOIOMMU_IOMMU: %sPresent\n",
		       ioctl(container, VFIO_CHECK_EXTENSION, VFIO_NOIOMMU_IOMMU) ?
		       "" : "Not ");
	}

	ret = ioctl(container, VFIO_SET_IOMMU, noiommu ?
		    VFIO_TYPE1_IOMMU : VFIO_NOIOMMU_IOMMU);
	if (!ret) {
		printf("Incorrectly allowed %s-iommu usage!\n", noiommu ?
		       "no" : "type1");
		return -1;
	}

	ret = ioctl(container, VFIO_SET_IOMMU, iommu_type);
	if (ret) {
		printf("Failed to set IOMMU: %d (%s)\n", ret, strerror(errno));

		return ret;
	}

	return 0;
}

static int vfio_container_open(void)
{
	int fd;

	fd = open("/dev/vfio/vfio", O_RDWR);
	if (fd < 0) {
		printf("Failed to open /dev/vfio/vfio : %d (%s)\n",
		       fd, strerror(errno));
	}
	return fd;
}

static int __vfio_group_attach(int groupid, int *container_out, int *group_out,
			       int iommu_type)
{
	int container, group;
	bool noiommu = VFIO_NOIOMMU_IOMMU == iommu_type;

	group = vfio_group_open(groupid, noiommu);
	if (group < 0)
		return -1;

	container = vfio_container_open();
	if (container < 0)
		return -1;

	if (vfio_group_set_container(group, container, iommu_type))
		return -1;

	if (container_out)
		*container_out = container;
	if (group_out)
		*group_out = group;
	return 0;
}

int vfio_group_attach(int groupid, int *container_out, int *group_out)
{
	return __vfio_group_attach(groupid, container_out, group_out,
				   VFIO_TYPE1_IOMMU);
}

static int __vfio_device_attach(const char *devname, int *container_out,
				int *device_out, int *group_out,
				int iommu_type)
{
	int container, group, groupid, device;

	groupid = vfio_device_get_groupid(devname);
	if (groupid < 0)
		return -1;

	if (__vfio_group_attach(groupid, &container, &group, iommu_type) < 0)
		return -1;

	if (device_out) {
		device = ioctl(group, VFIO_GROUP_GET_DEVICE_FD, devname);
		if (device < 0) {
			printf("Failed to get device %s: %d (%s)\n",
			       devname, container, strerror(errno));
			return -1;
		}
		*device_out = device;
	}

	if (container_out)
		*container_out = container;
	if (group_out)
		*group_out = group;
	return 0;
}

int vfio_device_attach(const char *devname, int *container_out, int *device_out,
		       int *group_out)
{
	return __vfio_device_attach(devname, container_out, device_out,
				    group_out, VFIO_TYPE1_IOMMU);
}

int vfio_device_attach_iommu_type(const char *devname, int *container_out,
				  int *device_out, int *group_out,
				  int iommu_type)
{
	return __vfio_device_attach(devname, container_out, device_out,
				    group_out, iommu_type);
}

#define ALIGN_UP(x, a)  (((x) + (a) - 1) & ~((a) - 1))

void *mmap_align(void *addr, size_t length, int prot, int flags,
		 int fd, off_t offset, size_t align)
{
	void *addr_align;
	void *addr_base;
	void *map;

	addr_base = mmap(NULL, length + align, PROT_NONE,
			 MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	if (addr_base == MAP_FAILED) {
		return addr_base;
	}

	addr_align = (void *)ALIGN_UP((uintptr_t)addr_base, (uintptr_t)align);
	munmap(addr_base, addr_align - addr_base);
	munmap(addr_align + length, align - (addr_align - addr_base));

	map = mmap(addr_align, length, prot, flags | MAP_FIXED, fd, offset);
	if (map != MAP_FAILED)
		madvise(map, length, MADV_HUGEPAGE);
	return map;
}
