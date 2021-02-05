PROGS = kvm-huge-guest-test \
	leaktest-legacy-kvm \
	vfio-correctness-tests \
	vfio-huge-guest-test \
	vfio-iommu-map-unmap \
	vfio-iommu-stress-test \
	vfio-noiommu-pci-device-open \
	vfio-pci-device-open \
	vfio-pci-device-open-igd \
	vfio-pci-device-open-sparse-mmap \
	vfio-pci-hot-reset

all: $(PROGS)

clean:
	rm -f *.o *~
	rm -f $(PROGS)

