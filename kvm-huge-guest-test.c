#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <linux/ioctl.h>

struct kvm_userspace_memory_region {
        unsigned int slot;
        unsigned int flags;
        unsigned long guest_phys_addr;
        unsigned long memory_size;
        unsigned long userspace_addr;
};

struct kvm_assigned_pci_dev {
	unsigned int assigned_dev_id;
	unsigned int busnr;
	unsigned int devfn;
	unsigned int flags;
#define KVM_DEV_ASSIGN_ENABLE_IOMMU	(1 << 0)
	unsigned int segnr;
	union {
		unsigned int reserved[11];
	};
};

#define KVMIO 0xAE
#define KVM_CAP_NR_MEMSLOTS 10

#define KVM_CHECK_EXTENSION	_IO(KVMIO, 0x03)

#define KVM_CREATE_VM		_IO(KVMIO, 0x01)

#define KVM_SET_USER_MEMORY_REGION	_IOW(KVMIO, 0x46, \
					struct kvm_userspace_memory_region)

#define KVM_ASSIGN_PCI_DEVICE	_IOR(KVMIO,  0x69, \
					struct kvm_assigned_pci_dev)

#define KVM_DEASSIGN_PCI_DEVICE	_IOW(KVMIO,  0x72, \
					struct kvm_assigned_pci_dev)

#define PCI_DEVFN(slot, func)	((((slot) & 0x1f) << 3) | ((func) & 0x07))
#define PCI_SLOT(devfn)		(((devfn) >> 3) & 0x1f)
#define PCI_FUNC(devfn)		((devfn) & 0x07)

#define MMAP_GB (4UL) /* >= 4GB */
#define MMAP_SIZE (MMAP_GB * 1024 * 1024 * 1024)
#define GUEST_GB (1024UL)

static unsigned int calc_assigned_dev_id(struct kvm_assigned_pci_dev *dev)
{
	return dev->segnr << 16 | dev->busnr << 8 | dev->devfn;
}

void usage(char *name)
{
	printf("usage: %s ssss:bb:dd.f\n", name);
	printf("\tssss: PCI segment, ex. 0000\n");
	printf("\tbb:   PCI bus, ex. 01\n");
	printf("\tdd:   PCI device, ex. 06\n");
	printf("\tf:    PCI function, ex. 0\n");
}

int main(int argc, char **argv)
{
	int kvmfd, vmfd, ret;
	unsigned long vaddr, i;
	int slot = 0, nr_slots, func;
	struct kvm_userspace_memory_region mem = {
		.flags = 0,
	};

	struct kvm_assigned_pci_dev dev = { 0 };

	if (argc != 2) {
		usage(argv[0]);
		return -1;
	}

	ret = sscanf(argv[1], "%04x:%02x:%02x.%d",
		     &dev.segnr, &dev.busnr, &slot, &func);
	if (ret != 4) {
		usage(argv[0]);
		return -1;
	}

	dev.devfn = PCI_DEVFN(slot, func);
	dev.assigned_dev_id = calc_assigned_dev_id(&dev);
	
	kvmfd = open("/dev/kvm", O_RDWR);
	if (kvmfd < 0) {
		printf("Failed to open /dev/kvm, %d (%s)\n",
		       ret, strerror(errno));
		return ret;
	}

	nr_slots = ioctl(kvmfd, KVM_CHECK_EXTENSION, KVM_CAP_NR_MEMSLOTS);
	if (nr_slots < 0) {
		printf("Failed to get slot count (%s)\n", strerror(errno));
		return nr_slots;
	}

	vmfd = ioctl(kvmfd, KVM_CREATE_VM, 0);
	if (vmfd < 0) {
		printf("Failed to create vm (%s)\n", strerror(errno));
		return vmfd;
	}

	vaddr = (unsigned long)mmap(0, MMAP_SIZE, PROT_READ | PROT_WRITE,
				    MAP_PRIVATE | MAP_ANONYMOUS, 0, 0);
	if (!vaddr) {
		printf("Failed to allocate vm memory\n");
		return -1;
	}

	dev.flags = KVM_DEV_ASSIGN_ENABLE_IOMMU;

	printf("Assigning\n");
	ret = ioctl(vmfd, KVM_ASSIGN_PCI_DEVICE, &dev);
	if (ret) {
		printf("failed to assign device %d (%s)\n", ret,
		       strerror(errno));
		return ret;
	}


	printf("Mapping 0-640K");
	fflush(stdout);
	mem.memory_size = 640 * 1024;
	mem.guest_phys_addr = 0;
	mem.userspace_addr = vaddr;
	mem.slot = slot++;

	ret = ioctl(vmfd, KVM_SET_USER_MEMORY_REGION, &mem);
	if (ret) {
		printf("Failed to add memory %d (%s)\n", slot - 1,
		       strerror(errno));
		return ret;
	}
	printf(".\n");
	fflush(stdout);

	printf("Mapping low memory");
	fflush(stdout);
	mem.memory_size = (3UL * 1024 * 1024 * 1024) - (1024 * 1024);
	mem.guest_phys_addr = 1024 * 1024;
	mem.userspace_addr = vaddr + mem.guest_phys_addr;
	mem.slot = slot++;

	ret = ioctl(vmfd, KVM_SET_USER_MEMORY_REGION, &mem);
	if (ret) {
		printf("Failed to add memory %d (%s)\n", slot - 1,
		       strerror(errno));
		return ret;
	}
	printf(".\n");
	fflush(stdout);

	printf("Mapping high memory");
	fflush(stdout);
	mem.memory_size = MMAP_SIZE;
	mem.guest_phys_addr = 4UL * 1024 * 1024 * 1024;
	mem.userspace_addr = vaddr;
	while (slot < nr_slots &&
	       mem.guest_phys_addr < GUEST_GB * 1024 * 1024 * 1024) {
		mem.slot = slot++;
		ret = ioctl(vmfd, KVM_SET_USER_MEMORY_REGION, &mem);
		if (ret) {
			printf("Failed to add memory %d (%s)\n", slot - 1,
			       strerror(errno));
			return ret;
		}
		printf(".");
		fflush(stdout);
		mem.guest_phys_addr += MMAP_SIZE;
	}
	printf("\n");
	if (slot == nr_slots)
		printf("Out of slots @%ldGB\n", mem.guest_phys_addr >> 30);

	return 0;
}
