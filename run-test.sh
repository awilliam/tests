#!/bin/bash

me=${0##*/}

device=${1:-"0000:08:10.0"}
groupid=$(basename $(readlink /sys/bus/pci/devices/$device/iommu_group))

set -x

./vfio-correctness-tests $groupid
./vfio-huge-guest-test $groupid
./vfio-iommu-map-unmap $device
./vfio-iommu-stress-test $device
./vfio-noiommu-pci-device-open $device
./vfio-pci-device-dma-map $device
./vfio-pci-device-open $device
./vfio-pci-device-open-igd $device
./vfio-pci-device-open-sparse-mmap $device
./vfio-pci-hot-reset $device
./vfio-pci-huge-fault-race $device
./iommufd-pci-device-open $device
