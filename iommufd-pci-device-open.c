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
#include <linux/iommufd.h>
#include <linux/vfio.h>

#include "utils.h"

void usage(char *name)
{
        printf("usage: %s <ssss:bb:dd.f>\n", name);
}

int main(int argc, char **argv)
{
	const char *devname;
        int i, ret, device, iommufd;

        struct vfio_device_info device_info = {
                .argsz = sizeof(device_info)
        };
        struct vfio_region_info region_info = {
                .argsz = sizeof(region_info)
        };

        if (argc < 2) {
                usage(argv[0]);
                return -1;
        }
	
	devname = argv[1];

        device = vfio_device_iommufd_getfd(devname);
        if (device < 0)
                return -1;

        struct vfio_device_bind_iommufd bind = {
            .argsz = sizeof(bind),
            .flags = 0,
        };
        struct iommu_ioas_alloc alloc_data  = {
            .size = sizeof(alloc_data),
            .flags = 0,
        };
        struct vfio_device_attach_iommufd_pt attach_data = {
            .argsz = sizeof(attach_data),
            .flags = 0,
        };
        struct iommu_ioas_map map = {
            .size = sizeof(map),
            .flags = IOMMU_IOAS_MAP_READABLE |
                IOMMU_IOAS_MAP_WRITEABLE |
                IOMMU_IOAS_MAP_FIXED_IOVA,
            .__reserved = 0,
        };

        iommufd = open("/dev/iommu", O_RDWR);
        if (iommufd < 0) {
                printf("Failed to open /dev/iommufd, %d (%s)\n",
                       iommufd, strerror(errno));
                return iommufd;
        }

        bind.iommufd = iommufd; // negative value means vfio-noiommu mode
        ret = ioctl(device, VFIO_DEVICE_BIND_IOMMUFD, &bind);
        if (ret < 0) {
                printf("Failed VFIO_DEVICE_BIND_IOMMUFD %d (%s)\n",
                       ret, strerror(errno));
                return ret;
        }

        printf("Bind to IOMMUFD %d with dev_id %d\n", iommufd, bind.out_devid);

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
                if (region_info.flags & VFIO_REGION_INFO_FLAG_MMAP) {
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

        ret = ioctl(iommufd, IOMMU_IOAS_ALLOC, &alloc_data);
        if (ret < 0) {
                printf("Failed IOMMU_IOAS_ALLOC %d (%s)\n",
                       ret, strerror(errno));
                return ret;
        }

        attach_data.pt_id = alloc_data.out_ioas_id;
        ret = ioctl(device, VFIO_DEVICE_ATTACH_IOMMUFD_PT, &attach_data);
        if (ret < 0) {
                printf("Failed VFIO_DEVICE_ATTACH_IOMMUFD_PT ioas_id %d %d (%s)\n",
                       attach_data.pt_id, ret, strerror(errno));
                return ret;
        }

        printf("Attached IOMMUFD %d ioas %d hwpt %d\n", iommufd, alloc_data.out_ioas_id, attach_data.pt_id);

        /* Allocate some space and setup a DMA mapping */
        map.user_va = (int64_t)mmap(0, 1024 * 1024, PROT_READ | PROT_WRITE,
                MAP_PRIVATE | MAP_ANONYMOUS, 0, 0);
        map.iova = 0; /* 1MB starting at 0x0 from device view */
        map.length = 1024 * 1024;
        map.ioas_id = alloc_data.out_ioas_id;;

        ret = ioctl(iommufd, IOMMU_IOAS_MAP, &map);
        if (ret < 0) {
                printf("Failed VFIO_DEVICE_ATTACH_IOMMUFD_PT ioas_id %d %d (%s)\n",
                       attach_data.pt_id, ret, strerror(errno));
                return ret;
        }
        printf("Mapped user_va %llx size %llx to iova %llx in ioas %d\n", map.user_va, map.length, map.iova, map.ioas_id);

        struct vfio_pci_hot_reset_info *reset_info;
        struct vfio_pci_dependent_device *devices;
        struct vfio_pci_hot_reset *reset;
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

        printf("Hot reset dependent device count: %d\n", reset_info->count);

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
                printf("%d: %04x:%02x:%02x.%d devid %d\n", i,
                                devices[i].segment, devices[i].bus,
                                devices[i].devfn >> 3, devices[i].devfn & 7,
                                devices[i].devid);

        if (!(reset_info->flags & VFIO_PCI_HOT_RESET_FLAG_DEV_ID)) {
                printf("VFIO_PCI_HOT_RESET_FLAG_DEV_ID should be set for IOMMUFD\n");
                return -1;
        }

        int unowned_cnt = 0;
        if (!(reset_info->flags & VFIO_PCI_HOT_RESET_FLAG_DEV_ID_OWNED)) {
                for (i = 0; i < reset_info->count; i++) {
                        if (devices[i].devid == VFIO_PCI_DEVID_NOT_OWNED) {
                                unowned_cnt++;
                                printf("Cannot reset device, "
                                        "depends on device %04x:%02x:%02x.%x "
                                        "which is not owned.\n",
                                        devices[i].segment, devices[i].bus,
                                        devices[i].devfn >> 3, devices[i].devfn & 7);
                        }
                }
                if (!unowned_cnt) {
                        printf("flags mismatch with data field, "
                                "VFIO_PCI_HOT_RESET_FLAG_DEV_ID_OWNED claimed but "
                                "no VFIO_PCI_DEVID_NOT_OWNED\n");
                        return -1;
                }
                return 0;
        }


        printf("Attempting reset: ");
        fflush(stdout);

        /* Use zero length array for hot reset with iommufd backend */
        reset = malloc(sizeof(*reset));
        reset->argsz = sizeof(*reset);

        /* Bus reset! */
        ret = ioctl(device, VFIO_DEVICE_PCI_HOT_RESET, reset);

        ret = ioctl(device, VFIO_DEVICE_PCI_HOT_RESET, reset);
        printf("Hot reset: %s\n", ret ? "Failed" : "Pass");

        printf("Press any key to exit\n");
        fgetc(stdin);

        return 0;
}
