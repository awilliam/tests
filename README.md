summary :
VFIO test cases on ppc uses VFIO_SPAPR_TCE_v2_IOMMU.
read more on https://docs.kernel.org/driver-api/vfio.html.

build & run :
$gcc -o vfio-pci-device-open vfio-pci-device-open.c
$gcc -o vfio-iommu-map-unmap vfio-iommu-map-unmap.c
$gcc -o vfio-huge-guest-test vfio-huge-guest-test.c

$ vfio-pci-device-open <iommu group id> <ssss:bb:dd.f>
$ vfio-iommu-map-unmap ssss:bb:dd.f 
$ vfio-huge-guest-test <iommu group id>
