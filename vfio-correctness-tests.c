/*
 * VFIO API definition
 *
 * Copyright (C) 2012 Red Hat, Inc.  All rights reserved.
 *     Author: Alex Williamson <alex.williamson@redhat.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#ifndef _UAPIVFIO_H
#define _UAPIVFIO_H

#include <linux/types.h>
#include <linux/ioctl.h>

#define VFIO_API_VERSION	0


/* Kernel & User level defines for VFIO IOCTLs. */

/* Extensions */

#define VFIO_TYPE1_IOMMU		1

/*
 * The IOCTL interface is designed for extensibility by embedding the
 * structure length (argsz) and flags into structures passed between
 * kernel and userspace.  We therefore use the _IO() macro for these
 * defines to avoid implicitly embedding a size into the ioctl request.
 * As structure fields are added, argsz will increase to match and flag
 * bits will be defined to indicate additional fields with valid data.
 * It's *always* the caller's responsibility to indicate the size of
 * the structure passed by setting argsz appropriately.
 */

#define VFIO_TYPE	(';')
#define VFIO_BASE	100

/* -------- IOCTLs for VFIO file descriptor (/dev/vfio/vfio) -------- */

/**
 * VFIO_GET_API_VERSION - _IO(VFIO_TYPE, VFIO_BASE + 0)
 *
 * Report the version of the VFIO API.  This allows us to bump the entire
 * API version should we later need to add or change features in incompatible
 * ways.
 * Return: VFIO_API_VERSION
 * Availability: Always
 */
#define VFIO_GET_API_VERSION		_IO(VFIO_TYPE, VFIO_BASE + 0)

/**
 * VFIO_CHECK_EXTENSION - _IOW(VFIO_TYPE, VFIO_BASE + 1, __u32)
 *
 * Check whether an extension is supported.
 * Return: 0 if not supported, 1 (or some other positive integer) if supported.
 * Availability: Always
 */
#define VFIO_CHECK_EXTENSION		_IO(VFIO_TYPE, VFIO_BASE + 1)

/**
 * VFIO_SET_IOMMU - _IOW(VFIO_TYPE, VFIO_BASE + 2, __s32)
 *
 * Set the iommu to the given type.  The type must be supported by an
 * iommu driver as verified by calling CHECK_EXTENSION using the same
 * type.  A group must be set to this file descriptor before this
 * ioctl is available.  The IOMMU interfaces enabled by this call are
 * specific to the value set.
 * Return: 0 on success, -errno on failure
 * Availability: When VFIO group attached
 */
#define VFIO_SET_IOMMU			_IO(VFIO_TYPE, VFIO_BASE + 2)

/* -------- IOCTLs for GROUP file descriptors (/dev/vfio/$GROUP) -------- */

/**
 * VFIO_GROUP_GET_STATUS - _IOR(VFIO_TYPE, VFIO_BASE + 3,
 *						struct vfio_group_status)
 *
 * Retrieve information about the group.  Fills in provided
 * struct vfio_group_info.  Caller sets argsz.
 * Return: 0 on succes, -errno on failure.
 * Availability: Always
 */
struct vfio_group_status {
	__u32	argsz;
	__u32	flags;
#define VFIO_GROUP_FLAGS_VIABLE		(1 << 0)
#define VFIO_GROUP_FLAGS_CONTAINER_SET	(1 << 1)
};
#define VFIO_GROUP_GET_STATUS		_IO(VFIO_TYPE, VFIO_BASE + 3)

/**
 * VFIO_GROUP_SET_CONTAINER - _IOW(VFIO_TYPE, VFIO_BASE + 4, __s32)
 *
 * Set the container for the VFIO group to the open VFIO file
 * descriptor provided.  Groups may only belong to a single
 * container.  Containers may, at their discretion, support multiple
 * groups.  Only when a container is set are all of the interfaces
 * of the VFIO file descriptor and the VFIO group file descriptor
 * available to the user.
 * Return: 0 on success, -errno on failure.
 * Availability: Always
 */
#define VFIO_GROUP_SET_CONTAINER	_IO(VFIO_TYPE, VFIO_BASE + 4)

/**
 * VFIO_GROUP_UNSET_CONTAINER - _IO(VFIO_TYPE, VFIO_BASE + 5)
 *
 * Remove the group from the attached container.  This is the
 * opposite of the SET_CONTAINER call and returns the group to
 * an initial state.  All device file descriptors must be released
 * prior to calling this interface.  When removing the last group
 * from a container, the IOMMU will be disabled and all state lost,
 * effectively also returning the VFIO file descriptor to an initial
 * state.
 * Return: 0 on success, -errno on failure.
 * Availability: When attached to container
 */
#define VFIO_GROUP_UNSET_CONTAINER	_IO(VFIO_TYPE, VFIO_BASE + 5)

/**
 * VFIO_GROUP_GET_DEVICE_FD - _IOW(VFIO_TYPE, VFIO_BASE + 6, char)
 *
 * Return a new file descriptor for the device object described by
 * the provided string.  The string should match a device listed in
 * the devices subdirectory of the IOMMU group sysfs entry.  The
 * group containing the device must already be added to this context.
 * Return: new file descriptor on success, -errno on failure.
 * Availability: When attached to container
 */
#define VFIO_GROUP_GET_DEVICE_FD	_IO(VFIO_TYPE, VFIO_BASE + 6)

/* --------------- IOCTLs for DEVICE file descriptors --------------- */

/**
 * VFIO_DEVICE_GET_INFO - _IOR(VFIO_TYPE, VFIO_BASE + 7,
 *						struct vfio_device_info)
 *
 * Retrieve information about the device.  Fills in provided
 * struct vfio_device_info.  Caller sets argsz.
 * Return: 0 on success, -errno on failure.
 */
struct vfio_device_info {
	__u32	argsz;
	__u32	flags;
#define VFIO_DEVICE_FLAGS_RESET	(1 << 0)	/* Device supports reset */
#define VFIO_DEVICE_FLAGS_PCI	(1 << 1)	/* vfio-pci device */
	__u32	num_regions;	/* Max region index + 1 */
	__u32	num_irqs;	/* Max IRQ index + 1 */
};
#define VFIO_DEVICE_GET_INFO		_IO(VFIO_TYPE, VFIO_BASE + 7)

/**
 * VFIO_DEVICE_GET_REGION_INFO - _IOWR(VFIO_TYPE, VFIO_BASE + 8,
 *				       struct vfio_region_info)
 *
 * Retrieve information about a device region.  Caller provides
 * struct vfio_region_info with index value set.  Caller sets argsz.
 * Implementation of region mapping is bus driver specific.  This is
 * intended to describe MMIO, I/O port, as well as bus specific
 * regions (ex. PCI config space).  Zero sized regions may be used
 * to describe unimplemented regions (ex. unimplemented PCI BARs).
 * Return: 0 on success, -errno on failure.
 */
struct vfio_region_info {
	__u32	argsz;
	__u32	flags;
#define VFIO_REGION_INFO_FLAG_READ	(1 << 0) /* Region supports read */
#define VFIO_REGION_INFO_FLAG_WRITE	(1 << 1) /* Region supports write */
#define VFIO_REGION_INFO_FLAG_MMAP	(1 << 2) /* Region supports mmap */
	__u32	index;		/* Region index */
	__u32	resv;		/* Reserved for alignment */
	__u64	size;		/* Region size (bytes) */
	__u64	offset;		/* Region offset from start of device fd */
};
#define VFIO_DEVICE_GET_REGION_INFO	_IO(VFIO_TYPE, VFIO_BASE + 8)

/**
 * VFIO_DEVICE_GET_IRQ_INFO - _IOWR(VFIO_TYPE, VFIO_BASE + 9,
 *				    struct vfio_irq_info)
 *
 * Retrieve information about a device IRQ.  Caller provides
 * struct vfio_irq_info with index value set.  Caller sets argsz.
 * Implementation of IRQ mapping is bus driver specific.  Indexes
 * using multiple IRQs are primarily intended to support MSI-like
 * interrupt blocks.  Zero count irq blocks may be used to describe
 * unimplemented interrupt types.
 *
 * The EVENTFD flag indicates the interrupt index supports eventfd based
 * signaling.
 *
 * The MASKABLE flags indicates the index supports MASK and UNMASK
 * actions described below.
 *
 * AUTOMASKED indicates that after signaling, the interrupt line is
 * automatically masked by VFIO and the user needs to unmask the line
 * to receive new interrupts.  This is primarily intended to distinguish
 * level triggered interrupts.
 *
 * The NORESIZE flag indicates that the interrupt lines within the index
 * are setup as a set and new subindexes cannot be enabled without first
 * disabling the entire index.  This is used for interrupts like PCI MSI
 * and MSI-X where the driver may only use a subset of the available
 * indexes, but VFIO needs to enable a specific number of vectors
 * upfront.  In the case of MSI-X, where the user can enable MSI-X and
 * then add and unmask vectors, it's up to userspace to make the decision
 * whether to allocate the maximum supported number of vectors or tear
 * down setup and incrementally increase the vectors as each is enabled.
 */
struct vfio_irq_info {
	__u32	argsz;
	__u32	flags;
#define VFIO_IRQ_INFO_EVENTFD		(1 << 0)
#define VFIO_IRQ_INFO_MASKABLE		(1 << 1)
#define VFIO_IRQ_INFO_AUTOMASKED	(1 << 2)
#define VFIO_IRQ_INFO_NORESIZE		(1 << 3)
	__u32	index;		/* IRQ index */
	__u32	count;		/* Number of IRQs within this index */
};
#define VFIO_DEVICE_GET_IRQ_INFO	_IO(VFIO_TYPE, VFIO_BASE + 9)

/**
 * VFIO_DEVICE_SET_IRQS - _IOW(VFIO_TYPE, VFIO_BASE + 10, struct vfio_irq_set)
 *
 * Set signaling, masking, and unmasking of interrupts.  Caller provides
 * struct vfio_irq_set with all fields set.  'start' and 'count' indicate
 * the range of subindexes being specified.
 *
 * The DATA flags specify the type of data provided.  If DATA_NONE, the
 * operation performs the specified action immediately on the specified
 * interrupt(s).  For example, to unmask AUTOMASKED interrupt [0,0]:
 * flags = (DATA_NONE|ACTION_UNMASK), index = 0, start = 0, count = 1.
 *
 * DATA_BOOL allows sparse support for the same on arrays of interrupts.
 * For example, to mask interrupts [0,1] and [0,3] (but not [0,2]):
 * flags = (DATA_BOOL|ACTION_MASK), index = 0, start = 1, count = 3,
 * data = {1,0,1}
 *
 * DATA_EVENTFD binds the specified ACTION to the provided __s32 eventfd.
 * A value of -1 can be used to either de-assign interrupts if already
 * assigned or skip un-assigned interrupts.  For example, to set an eventfd
 * to be trigger for interrupts [0,0] and [0,2]:
 * flags = (DATA_EVENTFD|ACTION_TRIGGER), index = 0, start = 0, count = 3,
 * data = {fd1, -1, fd2}
 * If index [0,1] is previously set, two count = 1 ioctls calls would be
 * required to set [0,0] and [0,2] without changing [0,1].
 *
 * Once a signaling mechanism is set, DATA_BOOL or DATA_NONE can be used
 * with ACTION_TRIGGER to perform kernel level interrupt loopback testing
 * from userspace (ie. simulate hardware triggering).
 *
 * Setting of an event triggering mechanism to userspace for ACTION_TRIGGER
 * enables the interrupt index for the device.  Individual subindex interrupts
 * can be disabled using the -1 value for DATA_EVENTFD or the index can be
 * disabled as a whole with: flags = (DATA_NONE|ACTION_TRIGGER), count = 0.
 *
 * Note that ACTION_[UN]MASK specify user->kernel signaling (irqfds) while
 * ACTION_TRIGGER specifies kernel->user signaling.
 */
struct vfio_irq_set {
	__u32	argsz;
	__u32	flags;
#define VFIO_IRQ_SET_DATA_NONE		(1 << 0) /* Data not present */
#define VFIO_IRQ_SET_DATA_BOOL		(1 << 1) /* Data is bool (u8) */
#define VFIO_IRQ_SET_DATA_EVENTFD	(1 << 2) /* Data is eventfd (s32) */
#define VFIO_IRQ_SET_ACTION_MASK	(1 << 3) /* Mask interrupt */
#define VFIO_IRQ_SET_ACTION_UNMASK	(1 << 4) /* Unmask interrupt */
#define VFIO_IRQ_SET_ACTION_TRIGGER	(1 << 5) /* Trigger interrupt */
	__u32	index;
	__u32	start;
	__u32	count;
	__u8	data[];
};
#define VFIO_DEVICE_SET_IRQS		_IO(VFIO_TYPE, VFIO_BASE + 10)

#define VFIO_IRQ_SET_DATA_TYPE_MASK	(VFIO_IRQ_SET_DATA_NONE | \
					 VFIO_IRQ_SET_DATA_BOOL | \
					 VFIO_IRQ_SET_DATA_EVENTFD)
#define VFIO_IRQ_SET_ACTION_TYPE_MASK	(VFIO_IRQ_SET_ACTION_MASK | \
					 VFIO_IRQ_SET_ACTION_UNMASK | \
					 VFIO_IRQ_SET_ACTION_TRIGGER)
/**
 * VFIO_DEVICE_RESET - _IO(VFIO_TYPE, VFIO_BASE + 11)
 *
 * Reset a device.
 */
#define VFIO_DEVICE_RESET		_IO(VFIO_TYPE, VFIO_BASE + 11)

/*
 * The VFIO-PCI bus driver makes use of the following fixed region and
 * IRQ index mapping.  Unimplemented regions return a size of zero.
 * Unimplemented IRQ types return a count of zero.
 */

enum {
	VFIO_PCI_BAR0_REGION_INDEX,
	VFIO_PCI_BAR1_REGION_INDEX,
	VFIO_PCI_BAR2_REGION_INDEX,
	VFIO_PCI_BAR3_REGION_INDEX,
	VFIO_PCI_BAR4_REGION_INDEX,
	VFIO_PCI_BAR5_REGION_INDEX,
	VFIO_PCI_ROM_REGION_INDEX,
	VFIO_PCI_CONFIG_REGION_INDEX,
	/*
	 * Expose VGA regions defined for PCI base class 03, subclass 00.
	 * This includes I/O port ranges 0x3b0 to 0x3bb and 0x3c0 to 0x3df
	 * as well as the MMIO range 0xa0000 to 0xbffff.  Each implemented
	 * range is found at it's identity mapped offset from the region
	 * offset, for example 0x3b0 is region_info.offset + 0x3b0.  Areas
	 * between described ranges are unimplemented.
	 */
	VFIO_PCI_VGA_REGION_INDEX,
	VFIO_PCI_NUM_REGIONS
};

enum {
	VFIO_PCI_INTX_IRQ_INDEX,
	VFIO_PCI_MSI_IRQ_INDEX,
	VFIO_PCI_MSIX_IRQ_INDEX,
	VFIO_PCI_NUM_IRQS
};

/* -------- API for Type1 VFIO IOMMU -------- */

/**
 * VFIO_IOMMU_GET_INFO - _IOR(VFIO_TYPE, VFIO_BASE + 12, struct vfio_iommu_info)
 *
 * Retrieve information about the IOMMU object. Fills in provided
 * struct vfio_iommu_info. Caller sets argsz.
 *
 * XXX Should we do these by CHECK_EXTENSION too?
 */
struct vfio_iommu_type1_info {
	__u32	argsz;
	__u32	flags;
#define VFIO_IOMMU_INFO_PGSIZES (1 << 0)	/* supported page sizes info */
	__u64	iova_pgsizes;		/* Bitmap of supported page sizes */
};

#define VFIO_IOMMU_GET_INFO _IO(VFIO_TYPE, VFIO_BASE + 12)

/**
 * VFIO_IOMMU_MAP_DMA - _IOW(VFIO_TYPE, VFIO_BASE + 13, struct vfio_dma_map)
 *
 * Map process virtual addresses to IO virtual addresses using the
 * provided struct vfio_dma_map. Caller sets argsz. READ &/ WRITE required.
 */
struct vfio_iommu_type1_dma_map {
	__u32	argsz;
	__u32	flags;
#define VFIO_DMA_MAP_FLAG_READ (1 << 0)		/* readable from device */
#define VFIO_DMA_MAP_FLAG_WRITE (1 << 1)	/* writable from device */
	__u64	vaddr;				/* Process virtual address */
	__u64	iova;				/* IO virtual address */
	__u64	size;				/* Size of mapping (bytes) */
};

#define VFIO_IOMMU_MAP_DMA _IO(VFIO_TYPE, VFIO_BASE + 13)

/**
 * VFIO_IOMMU_UNMAP_DMA - _IOWR(VFIO_TYPE, VFIO_BASE + 14,
 *							struct vfio_dma_unmap)
 *
 * Unmap IO virtual addresses using the provided struct vfio_dma_unmap.
 * Caller sets argsz.  The actual unmapped size is returned in the size
 * field.  No guarantee is made to the user that arbitrary unmaps of iova
 * or size different from those used in the original mapping call will
 * succeed.
 */
struct vfio_iommu_type1_dma_unmap {
	__u32	argsz;
	__u32	flags;
	__u64	iova;				/* IO virtual address */
	__u64	size;				/* Size of mapping (bytes) */
};

#define VFIO_IOMMU_UNMAP_DMA _IO(VFIO_TYPE, VFIO_BASE + 14)

#endif /* _UAPIVFIO_H */

#include <errno.h>
#include <libgen.h>
#include <fcntl.h>
#include <libgen.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/vfs.h>

#include <linux/ioctl.h>

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
			printf("Failed to map @0x%lx(%s)\n",
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
			printf("Error, allowed to remap @0x%lx(%s)\n",
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
			printf("Failed to unmap @0x%lx(%s)\n",
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
			printf("Error, allowed to re-unmap @0x%lx(%s)\n",
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
			printf("Failed to backwards map @0x%lx(%s)\n",
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
			printf("Failed to backwards unmap @0x%lx(%s)\n",
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
			printf("Failed even checker map @0x%lx(%s)\n",
			       dma_map.iova, strerror(errno));
			return ret;
		}
	}
	for (dma_map.vaddr = vaddr + pagesize, dma_map.iova = pagesize;
	     dma_map.iova < size;
	     dma_map.iova += (pagesize * 2), dma_map.vaddr += (pagesize * 2)) {
		ret = ioctl(fd, VFIO_IOMMU_MAP_DMA, &dma_map);
		if (ret) {
			printf("Failed odd checker map @0x%lx(%s)\n",
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
			printf("Failed even checker unmap @0x%lx(%s)\n",
			       dma_unmap.iova, strerror(errno));
			return ret;
		}
	}
	for (dma_unmap.iova = pagesize;
	     dma_unmap.iova < size;
	     dma_unmap.iova += (pagesize * 2)) {
		ret = ioctl(fd, VFIO_IOMMU_UNMAP_DMA, &dma_unmap);
		if (ret || dma_unmap.size != pagesize) {
			printf("Failed odd checker unmap @0x%lx(%s)\n",
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
			printf("Failed even backward checker map @0x%lx(%s)\n",
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
			printf("Failed odd backward checker map @0x%lx(%s)\n",
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
			printf("Failed even backward checker unmap @0x%lx(%s)\n",
			       dma_unmap.iova, strerror(errno));
			return ret;
		}
	}
	for (dma_unmap.iova = size - (pagesize * 2);
	     dma_unmap.iova < size;
	     dma_unmap.iova -= (pagesize * 2)) {
		ret = ioctl(fd, VFIO_IOMMU_UNMAP_DMA, &dma_unmap);
		if (ret || dma_unmap.size != pagesize) {
			printf("Failed odd backward checker unmap @0x%lx(%s)\n",
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
		printf("Failed to map @0x%lx(%s)\n",
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
		printf("Error, allowed to remap @0x%lx(%s)\n",
		       dma_map.iova, strerror(errno));
		return ret;
	}

	/* unmap it */
	dma_unmap.iova = 0;
	dma_unmap.size = size;
	ret = ioctl(fd, VFIO_IOMMU_UNMAP_DMA, &dma_unmap);
	if (ret || dma_unmap.size != size) {
		printf("Failed to unmap @0x%lx(%s)\n",
		       dma_unmap.iova, strerror(errno));
		return ret;
	}

	/* map it again */
	dma_map.vaddr = vaddr;
	dma_map.iova = 0;
	dma_map.size = size;
	ret = ioctl(fd, VFIO_IOMMU_MAP_DMA, &dma_map);
	if (ret) {
		printf("Failed to map @0x%lx(%s)\n",
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
			printf("Failed to unmap @0x%lx(%s)\n",
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
		printf("(unmaps 0x%lx, biggest page 0x%lx)\n",
		       unmaps, biggest_page);
	return 0;
}

int main(int argc, char **argv)
{
	int ret, container, group, groupid, fd = -1;
	int sp = false;
	char path[PATH_MAX], mempath[PATH_MAX] = "";
	unsigned long i, vaddr;
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

	if (argc > 2) {
		ret = sscanf(argv[2], "%s", mempath);
		if (ret != 1) {
			usage(argv[0]);
			return -1;
		}
	}

	container = open("/dev/vfio/vfio", O_RDWR);
	if (container < 0) {
		printf("Failed to open /dev/vfio/vfio, %d (%s)\n",
		       container, strerror(errno));
		return container;
	}

	snprintf(path, sizeof(path), "/dev/vfio/%d", groupid);
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

	ret = ioctl(group, VFIO_GROUP_SET_CONTAINER, &container);
	if (ret) {
		printf("Failed to set group container\n");
		return ret;
	}

	ret = ioctl(container, VFIO_SET_IOMMU, VFIO_TYPE1_IOMMU);
	if (ret) {
		printf("Failed to set IOMMU\n");
		return ret;
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

		printf("Using %dK page size, %dK huge page size\n",
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
