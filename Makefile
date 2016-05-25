obj-m := vfio-kprobe-unmask-always-injects.o

KDIR := /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)

default:
	$(MAKE) -C $(KDIR) M=$(PWD) modules
	$(CC) -o vfio-pci-intx-race vfio-pci-intx-race.c

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean
	rm -f vfio-pci-intx-race

modules_install:
	$(MAKE) -C $(KDIR) M=$(PWD) modules_install
