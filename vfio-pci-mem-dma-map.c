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

void usage(char *name)
{
	printf("usage: %s <ssss:bb:dd.f> <size GB> [hugetlb path]\n", name);
}

#define HIGH_MEM (4ul * 1024 * 1024 * 1024)
#define LOOP 10

int main(int argc, char **argv)
{
	int container, device, ret, huge_fd = -1, i;
	int prot = PROT_READ | PROT_WRITE;
	int flags = MAP_ANONYMOUS;
	char mempath[PATH_MAX] = "";
	unsigned long size_gb, map_total, unmap_total, start, elapsed;
	float secs;
	void *map;
	struct vfio_iommu_type1_dma_map dma_map = {
		.argsz = sizeof(dma_map),
		.iova = HIGH_MEM,
		.flags = VFIO_DMA_MAP_FLAG_READ | VFIO_DMA_MAP_FLAG_WRITE,
	};
	struct vfio_iommu_type1_dma_unmap dma_unmap = {
		.argsz = sizeof(dma_map),
		.iova = HIGH_MEM,
	};

	if (argc < 3 || argc > 4) {
		usage(argv[0]);
		return -1;
	}

	if (vfio_device_attach(argv[1], &container, &device))
		return -1;

	if (sscanf(argv[2], "%ld", &size_gb) != 1) {
		usage(argv[0]);
		return -1;
	}
	dma_map.size = dma_unmap.size = size_gb << 30;

	if (argc == 4) {
		struct statfs fs;

		if (sscanf(argv[3], "%s", mempath) != 1) {
			usage(argv[0]);
			return -1;
		}

		do {
			ret = statfs(mempath, &fs);
		} while (ret != 0 && errno == EINTR);

		if (ret) {
			printf("Can't statfs on %s\n", mempath);
			return -1;
		}

		printf("Using %ldMB huge page size on %s\n",
		       fs.f_bsize >> 20, mempath);

		sprintf(mempath + strlen(mempath), "/%s.XXXXXX", basename(argv[0]));
		huge_fd = mkstemp(mempath);
		if (huge_fd < 0) {
			printf("Failed to create hugetlb path %s\n", mempath);
			return -1;
		}

		ret = ftruncate(huge_fd, (off_t)(size_gb << 30));
		if (ret) {
			printf("Failed to allocate requested size %ldGB\n", size_gb);
			unlink(mempath);
			return -1;
		}

		flags &= ~MAP_ANONYMOUS;
	}

	map_total = unmap_total = 0;

	for (i = 0; i < LOOP; i++) {
		start = now_nsec();
		map = mmap(0, size_gb << 30, prot,
			   flags | (huge_fd == -1 ? MAP_PRIVATE : MAP_SHARED), huge_fd, 0);
		if (map == MAP_FAILED) {
			printf("Failed to allocate memory: %s\n", strerror(errno));
			if (huge_fd >= 0)
				unlink(mempath);
			return -1;
		}
		elapsed = now_nsec() - start;
		secs = (float)elapsed / NSEC_PER_SEC;
		fprintf(stderr, "%d: mmap in %.3fs\n", i, secs);
	
		dma_map.vaddr = (__u64)map;

		if (huge_fd == -1)
			madvise(map, size_gb << 30, MADV_HUGEPAGE);
	
		start = now_nsec();
		memset(map, 0, size_gb << 30);
		elapsed = now_nsec() - start;
		secs = (float)elapsed / NSEC_PER_SEC;
		fprintf(stderr, "%d: mmap populated in %.3fs\n", i, secs);

		start = now_nsec();
		ret = ioctl(container, VFIO_IOMMU_MAP_DMA, &dma_map);
		if (ret) {
			printf("VFIO MAP DMA ioctl failed: %s\n", strerror(errno));
			if (huge_fd >= 0)
				unlink(mempath);
			return -1;
		}
		elapsed = now_nsec() - start;
		secs = (float)elapsed / NSEC_PER_SEC;
		fprintf(stderr, "%d: VFIO MAP DMA in %.3fs\n", i, secs);
	
		map_total += elapsed;

		start = now_nsec();
		ret = ioctl(container, VFIO_IOMMU_UNMAP_DMA, &dma_unmap);
		if (ret) {
			printf("VFIO UNMAP DMA ioctl failed: %s\n", strerror(errno));
			if (huge_fd >= 0)
				unlink(mempath);
			return -1;
		}
		elapsed = now_nsec() - start;
		secs = (float)elapsed / NSEC_PER_SEC;
		fprintf(stderr, "%d: VFIO UNMAP DMA in %.3fs\n", i, secs);

		unmap_total += elapsed;

		start = now_nsec();
		munmap(map, size_gb << 30);
		elapsed = now_nsec() - start;
		secs = (float)elapsed / NSEC_PER_SEC;
		fprintf(stderr, "%d: munmap in %.3fs\n", i, secs);
	}

	printf("------- AVERAGE (%s) --------\n",
		huge_fd >= 0 ? "HUGETLBFS" : "MADV_HUGEPAGE");
	secs = (float)(map_total / i) / NSEC_PER_SEC;
	printf("VFIO MAP DMA in %.3f s (%.1f GB/s)\n", secs, (float)size_gb/secs);
	secs = (float)(unmap_total / i) / NSEC_PER_SEC;
	printf("VFIO UNMAP DMA in %.3f s (%.1f GB/s)\n", secs, (float)size_gb/secs);

	if (huge_fd >= 0) {
		unlink(mempath);
		return 0;
	}

	flags |= MAP_POPULATE | MAP_SHARED;
	map_total = unmap_total = 0;

	for (i = 0; i < LOOP; i++) {
		start = now_nsec();
		map = mmap(0, size_gb << 30, prot, flags, huge_fd, 0);
		if (map == MAP_FAILED) {
			printf("Failed to allocate memory: %s\n", strerror(errno));
			if (huge_fd >= 0)
				unlink(mempath);
			return -1;
		}
		elapsed = now_nsec() - start;
		secs = (float)elapsed / NSEC_PER_SEC;
		fprintf(stderr, "%d: mmap in %.3fs\n", i, secs);
	
		dma_map.vaddr = (__u64)map;

		start = now_nsec();
		ret = ioctl(container, VFIO_IOMMU_MAP_DMA, &dma_map);
		if (ret) {
			printf("VFIO MAP DMA ioctl failed: %s\n", strerror(errno));
			if (huge_fd >= 0)
				unlink(mempath);
			return -1;
		}
		elapsed = now_nsec() - start;
		secs = (float)elapsed / NSEC_PER_SEC;
		fprintf(stderr, "%d: VFIO MAP DMA in %.3fs\n", i, secs);
	
		map_total += elapsed;

		start = now_nsec();
		ret = ioctl(container, VFIO_IOMMU_UNMAP_DMA, &dma_unmap);
		if (ret) {
			printf("VFIO UNMAP DMA ioctl failed: %s\n", strerror(errno));
			if (huge_fd >= 0)
				unlink(mempath);
			return -1;
		}
		elapsed = now_nsec() - start;
		secs = (float)elapsed / NSEC_PER_SEC;
		fprintf(stderr, "%d: VFIO UNMAP DMA in %.3fs\n", i, secs);

		unmap_total += elapsed;

		start = now_nsec();
		munmap(map, size_gb << 30);
		elapsed = now_nsec() - start;
		secs = (float)elapsed / NSEC_PER_SEC;
		fprintf(stderr, "%d: munmap in %.3fs\n", i, secs);
	}

	printf("------- AVERAGE (MAP_POPULATE) --------\n");
	secs = (float)(map_total / i) / NSEC_PER_SEC;
	printf("VFIO MAP DMA in %.3f s (%.1f GB/s)\n", secs, (float)size_gb/secs);
	secs = (float)(unmap_total / i) / NSEC_PER_SEC;
	printf("VFIO UNMAP DMA in %.3f s (%.1f GB/s)\n", secs, (float)size_gb/secs);
	return 0;
}
