
#include <linux/types.h>
#include <linux/ioctl.h>
#include <stdlib.h>

#define VFIO_API_VERSION	0


/* Kernel & User level defines for VFIO IOCTLs. */

/* Extensions */

#define VFIO_TYPE1_IOMMU		1
#define VFIO_TYPE1v2_IOMMU		3
#define VFIO_SPAPR_TCE_v2_IOMMU		7
#define VFIO_NOIOMMU_IOMMU              8


#include <errno.h>
#include <libgen.h>
#include <fcntl.h>
#include <libgen.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
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
#include <asm/eeh.h>

#define CAP 0x0
#define VS 0x8
#define CC 0x14
#define CSTS 0x1c
#define AQA 0x24
#define ASQ 0x28
#define ACQ 0x30
#define S_DB 0x1000
#define C_DB 0x1004
#define S_DB_Q1 0x1008
#define C_DB_Q1 0x100c


void usage(char *name)
{
	printf("usage: %s <iommu group id> <ssss:bb:dd.f>\n", name);
}



	struct device_t {
    	struct vfio_region_info regs[VFIO_PCI_NUM_REGIONS];
    	struct vfio_irq_info irqs[VFIO_PCI_NUM_IRQS];
    	void* mmio_addr;  // mmio address (BAR0);
	} pcidev;

	struct vfio_region_info* bar_info;

	static inline void write_u32(struct device_t* dev, int offset, uint32_t value) {
		__asm__ volatile("" : : : "memory");
    		*((volatile uint32_t*)(dev->mmio_addr + offset)) = value;
	}

	static inline uint32_t read_u32(struct device_t* dev, int offset) {
		__asm__ volatile("" : : : "memory");
    		return *((volatile uint32_t*)(dev->mmio_addr + offset));
	}
static void eeh_dev_break_debugfs(char *bdf, int bdf_len)
{
  	int fd;
	printf("buf out : %s %d\n", bdf, bdf_len);
 	fd = open("/sys/kernel/debug/powerpc/eeh_dev_break", O_WRONLY);
  	write(fd, bdf, bdf_len);
  	close(fd);
}

int main(int argc, char **argv)
{
	int i, ret, container, group, device, groupid;
	char path[PATH_MAX], c;
	int seg, bus, dev, func;
	uint32_t phys_val =0, val;
	__u64	offset;
	ssize_t pret;
        uint32_t mask= 0b0100;
	unsigned int buf[512];

	struct vfio_group_status group_status = {
		.argsz = sizeof(group_status)
	};

	struct vfio_device_info device_info = {
		.argsz = sizeof(device_info)
	};

	struct vfio_region_info region_info = {
		.argsz = sizeof(region_info)
	};

	struct vfio_eeh_pe_op pe_op = {
		.argsz = sizeof(pe_op),
		.flags = 0
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

        printf("pre-SET_CONTAINER:\n");

#ifndef __PPC64__
        printf("VFIO_CHECK_EXTENSION VFIO_TYPE1_IOMMU: %sPresent\n",
               ioctl(container, VFIO_CHECK_EXTENSION, VFIO_TYPE1_IOMMU) ?
               "" : "Not ");
        printf("VFIO_CHECK_EXTENSION VFIO_NOIOMMU_IOMMU: %sPresent\n",
               ioctl(container, VFIO_CHECK_EXTENSION, VFIO_NOIOMMU_IOMMU) ?
               "" : "Not ");
#endif
	ret = ioctl(group, VFIO_GROUP_SET_CONTAINER, &container);
	if (ret) {
		printf("Failed to set group container\n");
		return ret;
	}

        printf("post-SET_CONTAINER:\n");
#ifndef __PPC64__
	printf("VFIO_CHECK_EXTENSION VFIO_TYPE1_IOMMU: %sPresent\n",
               ioctl(container, VFIO_CHECK_EXTENSION, VFIO_TYPE1_IOMMU) ?
               "" : "Not ");
        printf("VFIO_CHECK_EXTENSION VFIO_NOIOMMU_IOMMU: %sPresent\n",
               ioctl(container, VFIO_CHECK_EXTENSION, VFIO_NOIOMMU_IOMMU) ?
               "" : "Not ");

	ret = ioctl(container, VFIO_SET_IOMMU, VFIO_NOIOMMU_IOMMU);
	if (!ret) {
		printf("Incorrectly allowed no-iommu usage!\n");
		return -1;
	}

	ret = ioctl(container, VFIO_SET_IOMMU, VFIO_TYPE1_IOMMU);
	if (ret) {
		printf("Failed to set IOMMU\n");
		return ret;
	}
#else
        printf("VFIO_CHECK_EXTENSION VFIO_SPAPR_TCE_v2_IOMMU: %s Present\n",
		ioctl(container, VFIO_CHECK_EXTENSION, VFIO_SPAPR_TCE_v2_IOMMU) ?
                "" : "Not ");


        ret = ioctl(container, VFIO_SET_IOMMU, VFIO_SPAPR_TCE_v2_IOMMU);
        if (ret) {
                printf("Failed to set IOMMU\n");
                return ret;
        }
	printf("VFIO_SET_IOMMU to VFIO_SPAPR_TCE_v2_IOMMU successfull\n");

#endif

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
		pcidev.regs[i].argsz = sizeof(struct vfio_region_info);
		pcidev.regs[i].index = i;
		if (ioctl(device, VFIO_DEVICE_GET_REGION_INFO, &pcidev.regs[i])) {
			printf("Failed to get info\n");
			continue;
		}

		printf("size 0x%lx, offset 0x%lx, flags 0b%b\n",
		       (unsigned long)pcidev.regs[i].size,
		       (unsigned long)pcidev.regs[i].offset, region_info.flags);
		if (pcidev.regs[i].flags & VFIO_REGION_INFO_FLAG_MMAP) {
			void *map = mmap(NULL, (size_t)pcidev.regs[i].size,
					 PROT_READ, MAP_SHARED, device,
					 (off_t)pcidev.regs[i].offset);
			if (map == MAP_FAILED) {
				printf("mmap failed\n");
				continue;
			}

			printf("[");
			fwrite(map, 1, pcidev.regs[i].size > 16 ? 16 :
						pcidev.regs[i].size, stdout);
			printf("]\n");
			munmap(map, (size_t)pcidev.regs[i].size);
		}
		if (i==0) {
			offset = pcidev.regs[i].offset+4;
		}
	}

	/* MMIO mapings */
	bar_info = &pcidev.regs[VFIO_PCI_BAR0_REGION_INDEX];
	pcidev.mmio_addr = mmap(NULL, bar_info->size, PROT_READ | PROT_WRITE,
                          MAP_SHARED, device, bar_info->offset);

	/* read MMIO */
	for(i = 0; i < 0x10; i+=0x4 )
		printf("MMIO register offset 0x%X value 0x%X \n",i, read_u32(&pcidev, i));
	/*write_u32(&pcidev, 0x100c, 0x1);
    	printf("CAP  %x \n",read_u32(&pcidev, CAP));
    	printf("VS %x \n",read_u32(&pcidev, VS));
    	printf("CC %x \n",read_u32(&pcidev, CC));
    	printf("CSTS %x \n",read_u32(&pcidev, CSTS));
    	printf("AQA %x \n",read_u32(&pcidev, AQA));
    	printf("ASQ %x \n",read_u32(&pcidev, ASQ));
    	printf("ACQ %x \n",read_u32(&pcidev, ACQ));
    	printf("S_DB %x \n",read_u32(&pcidev, S_DB));
    	printf("C_DB %x \n",read_u32(&pcidev, C_DB));
    	printf("S_DB_Q1 %x \n",read_u32(&pcidev,S_DB_Q1));
    	printf("C_DB_Q1 %x \n",read_u32(&pcidev,C_DB_Q1 )); */

	/* Make sure EEH is supported */
	ret = ioctl(container, VFIO_CHECK_EXTENSION, VFIO_EEH);
	if(ret < 0) {
		printf("EEH not supported %d\n",errno);
		perror("");
		exit(0);
	}
	/* Enable the EEH functionality on the device */
	pe_op.op = VFIO_EEH_PE_ENABLE;
	if(ioctl(container, VFIO_EEH_PE_OP, &pe_op) ==-1) {
		printf("EEH enable on device failed\n");
		perror("");
	}
	/* Check the PE's state and make sure it's in functional state */
	pe_op.op = VFIO_EEH_PE_GET_STATE;
	if(ioctl(container, VFIO_EEH_PE_OP, &pe_op) ==-1) {
		perror("");

	}
	//pci read config
	printf("\ndump configuration space registers\n");
    	pret = pread(device, buf, 512, pcidev.regs[VFIO_PCI_CONFIG_REGION_INDEX].offset);
	if(pret < 0)
	{
		perror("read onfig: ");
	}
	else
	{
    		for(i=0; i<10; i++)
        		printf("%x \n",buf[i]);
		//printf("read config offset = %ld physical val 0x%X\n", offset, phys_val);
		c = getc(stdin);
	}


	/* Inject EEH error, which is expected to be caused by 32-bits
 	* config load.
 	*/
	pe_op.op = VFIO_EEH_PE_INJECT_ERR;
	pe_op.err.type = EEH_ERR_TYPE_32;
	pe_op.err.func = EEH_ERR_FUNC_LD_CFG_ADDR;
	pe_op.err.addr = 0ul;
	pe_op.err.mask = 0ul;
	pret = ioctl(container, VFIO_EEH_PE_OP, &pe_op);
	if(pret < 0) {
		perror("");
	char buf[]="0028:01:00.0"; 
  	int fd;
  	int len = sizeof(buf);
	printf("buf out : %s %d\n", buf, len);
 	//fd = open("/sys/kernel/debug/powerpc/eeh_dev_break", O_WRONLY);
  	//write(fd, &buf, (len-1));
  	//close(fd);
	eeh_dev_break_debugfs(buf, len);
	}


	pe_op.op = VFIO_EEH_PE_GET_STATE;
	ret = ioctl(container, VFIO_EEH_PE_OP, &pe_op);
	if(ret < 0) {
		perror("");
	}
 	printf("VFIO_EEH_PE_STATE after injectiong error %x ", ret);
	c = getc(stdin);

	/* When 0xFF's returned from reading PCI config space or IO BARs
 	* of the PCI device. Check the PE's state to see if that has been
	* frozen.
 	*/
	//write_u32(&pcidev, 0x100c, 0x1);
	//pci read config
	printf("\ndump configuration space registers\n");
    	pret = pread(device, buf, 512, pcidev.regs[VFIO_PCI_CONFIG_REGION_INDEX].offset);
	if(pret < 0)
	{
		perror("read onfig: ");
	}
	else
	{
    		for(i=0; i<10; i++)
        		printf("%x \n",buf[i]);
		//printf("read config offset = %ld physical val 0x%X\n", offset, phys_val);
		c = getc(stdin);
	}

	//pci read config
	pret = pread(device, &phys_val, 2,offset);
	if(pret < 0)
	{
		perror("read onfig: ");
	}
	else
	{
		printf("read config offset = %ld physical val 0x%X\n", offset, phys_val);
		c = getc(stdin);
	}
	/* read MMIO */
	for(i = 0; i < 0x10; i+=0x4 )
		printf("MMIO register offset 0x%X value 0x%X \n",i, read_u32(&pcidev, i));

	pe_op.op = VFIO_EEH_PE_GET_STATE;
	ret = ioctl(container, VFIO_EEH_PE_OP, &pe_op);
	if( ret < 0) {
		perror("");
	}
        printf("VFIO_EEH_PE_GET_STATE %d \n",ret);
	/* Waiting for pending PCI transactions to be completed and don't
 	* produce any more PCI traffic from/to the affected PE until
 	* recovery is finished.
 	*/

	/* Enable IO for the affected PE and collect logs. Usually, the
 	* standard part of PCI config space, AER registers are dumped
	 * as logs for further analysis.
	 */
	printf("VFIO_EEH_PE_UNFREEZE_IO\n");
	fgetc(stdin);
	pe_op.op = VFIO_EEH_PE_UNFREEZE_IO;
	ioctl(container, VFIO_EEH_PE_OP, &pe_op);

	/*
	 * Issue PE reset: hot or fundamental reset. Usually, hot reset
 	* is enough. However, the firmware of some PCI adapters would
	 * require fundamental reset.
 	*/
	printf("Press any key to exit\n");
	fgetc(stdin);
	pe_op.op = VFIO_EEH_PE_RESET_HOT;
	ioctl(container, VFIO_EEH_PE_OP, &pe_op);
	pe_op.op = VFIO_EEH_PE_RESET_DEACTIVATE;
	ioctl(container, VFIO_EEH_PE_OP, &pe_op);

	printf("Press any key to exit\n");
	fgetc(stdin);
	/* Configure the PCI bridges for the affected PE */
	pe_op.op = VFIO_EEH_PE_CONFIGURE;
	ioctl(container, VFIO_EEH_PE_OP, &pe_op);


	printf("Success\n");
	printf("Press any key to exit\n");
	fgetc(stdin);

	return 0;
}
