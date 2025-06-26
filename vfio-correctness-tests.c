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
	printf("usage: %s <iommu group id> [memory path]\n", name);
}

#define false 0
#define true 1

int pagesize_test(int fd, unsigned long vaddr,
		   unsigned long size, unsigned long pagesize)
{
	struct vfio_iommu_type1_dma_map dma_map = {
		.argsz = sizeof(dma_map),
		.flags = VFIO_DMA_MAP_FLAG_READ | VFIO_DMA_MAP_FLAG_WRITE,
		.size = pagesize,
	};
	struct vfio_iommu_type1_dma_unmap dma_unmap = {
		.argsz = sizeof(dma_unmap),
		.size = pagesize,
	};
	int ret;

	/* map it */
	for (dma_map.vaddr = vaddr, dma_map.iova = 0;
	     dma_map.iova < size;
	     dma_map.iova += pagesize, dma_map.vaddr += pagesize) {
		ret = ioctl(fd, VFIO_IOMMU_MAP_DMA, &dma_map);
		if (ret) {
			printf("Failed to map @0x%llx(%s)\n",
			       dma_map.iova, strerror(errno));
			return ret;
		}
	}

	/* attempt to remap it */
	for (dma_map.vaddr = vaddr, dma_map.iova = 0;
	     dma_map.iova < size;
	     dma_map.iova += pagesize, dma_map.vaddr += pagesize) {
		ret = ioctl(fd, VFIO_IOMMU_MAP_DMA, &dma_map);
		if (!ret) {
			printf("Error, allowed to remap @0x%llx(%s)\n",
			       dma_map.iova, strerror(errno));
			return ret;
		}
	}

	/* unmap it */
	for (dma_unmap.iova = 0;
	     dma_unmap.iova < size;
	     dma_unmap.iova += pagesize) {
		ret = ioctl(fd, VFIO_IOMMU_UNMAP_DMA, &dma_unmap);
		if (ret || dma_unmap.size != pagesize) {
			printf("Failed to unmap @0x%llx(%s)\n",
			       dma_unmap.iova, strerror(errno));
			return ret;
		}
	}

	/* attempt to re-unmap it */
	for (dma_unmap.iova = 0;
	     dma_unmap.iova < size;
	     dma_unmap.iova += pagesize) {
		dma_unmap.size = pagesize;
		ret = ioctl(fd, VFIO_IOMMU_UNMAP_DMA, &dma_unmap);
		if (ret || dma_unmap.size) {
			printf("Error, allowed to re-unmap @0x%llx(%s)\n",
			       dma_unmap.iova, strerror(errno));
			return ret;
		}
	}
	dma_unmap.size = pagesize;

	/* map it again, backwards*/
	for (dma_map.vaddr = vaddr + size - pagesize,
	     dma_map.iova = size - pagesize;
	     dma_map.iova < size;
	     dma_map.iova -= pagesize, dma_map.vaddr -= pagesize) {
		ret = ioctl(fd, VFIO_IOMMU_MAP_DMA, &dma_map);
		if (ret) {
			printf("Failed to backwards map @0x%llx(%s)\n",
			       dma_map.iova, strerror(errno));
			return ret;
		}
	}

	/* unmap it, backwards */
	for (dma_unmap.iova = size - pagesize;
	     dma_unmap.iova < size;
	     dma_unmap.iova -= pagesize) {
		ret = ioctl(fd, VFIO_IOMMU_UNMAP_DMA, &dma_unmap);
		if (ret || dma_unmap.size != pagesize) {
			printf("Failed to backwards unmap @0x%llx(%s)\n",
			       dma_unmap.iova, strerror(errno));
			return ret;
		}
	}

	/* map it again, checker board */
	for (dma_map.vaddr = vaddr, dma_map.iova = 0;
	     dma_map.iova < size;
	     dma_map.iova += (pagesize * 2), dma_map.vaddr += (pagesize * 2)) {
		ret = ioctl(fd, VFIO_IOMMU_MAP_DMA, &dma_map);
		if (ret) {
			printf("Failed even checker map @0x%llx(%s)\n",
			       dma_map.iova, strerror(errno));
			return ret;
		}
	}
	for (dma_map.vaddr = vaddr + pagesize, dma_map.iova = pagesize;
	     dma_map.iova < size;
	     dma_map.iova += (pagesize * 2), dma_map.vaddr += (pagesize * 2)) {
		ret = ioctl(fd, VFIO_IOMMU_MAP_DMA, &dma_map);
		if (ret) {
			printf("Failed odd checker map @0x%llx(%s)\n",
			       dma_map.iova, strerror(errno));
			return ret;
		}
	}

	/* unmap it, checker board */
	for (dma_unmap.iova = 0;
	     dma_unmap.iova < size;
	     dma_unmap.iova += (pagesize * 2)) {
		ret = ioctl(fd, VFIO_IOMMU_UNMAP_DMA, &dma_unmap);
		if (ret || dma_unmap.size != pagesize) {
			printf("Failed even checker unmap @0x%llx(%s)\n",
			       dma_unmap.iova, strerror(errno));
			return ret;
		}
	}
	for (dma_unmap.iova = pagesize;
	     dma_unmap.iova < size;
	     dma_unmap.iova += (pagesize * 2)) {
		ret = ioctl(fd, VFIO_IOMMU_UNMAP_DMA, &dma_unmap);
		if (ret || dma_unmap.size != pagesize) {
			printf("Failed odd checker unmap @0x%llx(%s)\n",
			       dma_unmap.iova, strerror(errno));
			return ret;
		}
	}

	/* map it again, backwards checker board */
	for (dma_map.vaddr = vaddr + size - pagesize,
	     dma_map.iova = size - pagesize;
	     dma_map.iova < size;
	     dma_map.iova -= (pagesize * 2), dma_map.vaddr -= (pagesize * 2)) {
		ret = ioctl(fd, VFIO_IOMMU_MAP_DMA, &dma_map);
		if (ret) {
			printf("Failed even backward checker map @0x%llx(%s)\n",
			       dma_map.iova, strerror(errno));
			return ret;
		}
	}
	for (dma_map.vaddr = vaddr + size - (pagesize * 2),
	     dma_map.iova = size - (pagesize * 2);
	     dma_map.iova < size;
	     dma_map.iova -= (pagesize * 2), dma_map.vaddr -= (pagesize * 2)) {
		ret = ioctl(fd, VFIO_IOMMU_MAP_DMA, &dma_map);
		if (ret) {
			printf("Failed odd backward checker map @0x%llx(%s)\n",
			       dma_map.iova, strerror(errno));
			return ret;
		}
	}

	/* unmap it, backwards checker board */
	for (dma_unmap.iova = size - pagesize;
	     dma_unmap.iova < size;
	     dma_unmap.iova -= (pagesize * 2)) {
		ret = ioctl(fd, VFIO_IOMMU_UNMAP_DMA, &dma_unmap);
		if (ret || dma_unmap.size != pagesize) {
			printf("Failed even backward checker unmap @0x%llx(%s)\n",
			       dma_unmap.iova, strerror(errno));
			return ret;
		}
	}
	for (dma_unmap.iova = size - (pagesize * 2);
	     dma_unmap.iova < size;
	     dma_unmap.iova -= (pagesize * 2)) {
		ret = ioctl(fd, VFIO_IOMMU_UNMAP_DMA, &dma_unmap);
		if (ret || dma_unmap.size != pagesize) {
			printf("Failed odd backward checker unmap @0x%llx(%s)\n",
			       dma_unmap.iova, strerror(errno));
			return ret;
		}
	}

	printf("pagesize test: PASSED\n");
	return 0;
}

int hugepage_test(int fd, unsigned long vaddr,
		  unsigned long size, unsigned long pagesize)
{
	struct vfio_iommu_type1_dma_map dma_map = {
		.argsz = sizeof(dma_map),
		.flags = VFIO_DMA_MAP_FLAG_READ | VFIO_DMA_MAP_FLAG_WRITE,
	};
	struct vfio_iommu_type1_dma_unmap dma_unmap = {
		.argsz = sizeof(dma_unmap),
	};
	int ret;
	int unmaps;
	unsigned long unmapped;
	unsigned long biggest_page;

	/* map it */
	dma_map.vaddr = vaddr;
	dma_map.iova = 0;
	dma_map.size = size;
	ret = ioctl(fd, VFIO_IOMMU_MAP_DMA, &dma_map);
	if (ret) {
		printf("Failed to map @0x%llx(%s)\n",
		       dma_map.iova, strerror(errno));
		if (errno == EBUSY)
			printf("If this is an AMD system, this may be a known bug\n");
		return ret;
	}

	/* attempt to remap it */
	dma_map.vaddr = vaddr;
	dma_map.iova = 0;
	dma_map.size = size;
	ret = ioctl(fd, VFIO_IOMMU_MAP_DMA, &dma_map);
	if (!ret) {
		printf("Error, allowed to remap @0x%llx(%s)\n",
		       dma_map.iova, strerror(errno));
		return ret;
	}

	/* unmap it */
	dma_unmap.iova = 0;
	dma_unmap.size = size;
	ret = ioctl(fd, VFIO_IOMMU_UNMAP_DMA, &dma_unmap);
	if (ret || dma_unmap.size != size) {
		printf("Failed to unmap @0x%llx(%s)\n",
		       dma_unmap.iova, strerror(errno));
		return ret;
	}

	/* map it again */
	dma_map.vaddr = vaddr;
	dma_map.iova = 0;
	dma_map.size = size;
	ret = ioctl(fd, VFIO_IOMMU_MAP_DMA, &dma_map);
	if (ret) {
		printf("Failed to map @0x%llx(%s)\n",
		       dma_map.iova, strerror(errno));
		return ret;
	}

	/* unmap it, backwards */
	unmaps = unmapped = biggest_page = 0;
	for (dma_unmap.iova = size - pagesize;
	     dma_unmap.iova < size;
	     dma_unmap.iova -= pagesize) {
		dma_unmap.size = pagesize;
		ret = ioctl(fd, VFIO_IOMMU_UNMAP_DMA, &dma_unmap);
		if (ret) {
			printf("Failed to unmap @0x%llx(%s)\n",
			       dma_unmap.iova, strerror(errno));
			return ret;
		}
		if (dma_unmap.size) {
			unmaps++;
			unmapped += dma_unmap.size;
			if (dma_unmap.size > biggest_page)
				biggest_page = dma_unmap.size;
		}
	}
	if (unmapped != size) {
		printf("Error, only unmapped 0x%lx of 0x%lx\n", unmapped, size);
		return -1;
	}

	printf("hugepage test: PASSED\n");
	if (unmaps > 1)
		printf("(unmaps 0x%x, biggest page 0x%lx)\n",
		       unmaps, biggest_page);
	return 0;
}

int main(int argc, char **argv)
{
	int ret, container, groupid, fd = -1;
	char path[PATH_MAX], mempath[PATH_MAX] = "";
	unsigned long vaddr;
	struct statfs fs;
	long hugepagesize, pagesize, mapsize;

	struct vfio_group_status group_status = {
		.argsz = sizeof(group_status)
	};
	struct vfio_iommu_type1_dma_map dma_map = {
		.argsz = sizeof(dma_map)
	};
	struct vfio_iommu_type1_dma_unmap dma_unmap = {
		.argsz = sizeof(dma_unmap)
	};

	if (argc < 2) {
		usage(argv[0]);
		return -1;
	}

	ret = sscanf(argv[1], "%d", &groupid);
	if (ret != 1) {
		usage(argv[0]);
		return -1;
	}

	if (vfio_group_attach(groupid, &container, NULL))
		return -1;

	if (argc > 2) {
		ret = sscanf(argv[2], "%s", mempath);
		if (ret != 1) {
			usage(argv[0]);
			return -1;
		}
	}

	hugepagesize = pagesize = getpagesize();

	if (strlen(mempath)) {
		do {
			ret = statfs(mempath, &fs);
		} while (ret != 0 && errno == EINTR);

		if (ret) {
			printf("Can't statfs on %s\n", mempath);
		} else
			hugepagesize = fs.f_bsize;

		printf("Using %ldK page size, %ldK huge page size\n",
		       pagesize >> 10, hugepagesize >> 10);

		sprintf(path, "%s/%s.XXXXXX", mempath, basename(argv[0]));
		fd = mkstemp(path);
		if (fd < 0)
			printf("Failed to open mempath file %s (%s)\n",
			       path, strerror(errno));
	}
	
	if (hugepagesize == pagesize)
		mapsize = 2 * 1024 * 1024;
	else
		mapsize = hugepagesize;

	if (fd < 0) {
		vaddr = (unsigned long)mmap(0, mapsize,
					    PROT_READ | PROT_WRITE,
					    MAP_PRIVATE | MAP_ANONYMOUS, 0, 0);
	} else {
		ftruncate(fd, mapsize);
		vaddr = (unsigned long)mmap(0, mapsize,
					    PROT_READ | PROT_WRITE,
					    MAP_POPULATE | MAP_SHARED, fd, 0);
	}

	if (!vaddr) {
		printf("Failed to allocate memory\n");
		return -1;
	}

	if (pagesize_test(container, vaddr, mapsize, pagesize)) {
		printf("pagesize test: FAILED\n");
		return -1;
	}

	if (hugepage_test(container, vaddr, mapsize, pagesize)) {
		printf("hugepage test: FAILED\n");
		return -1;
	}

	return 0;
}
