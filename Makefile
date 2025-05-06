CFLAGS := -g -Wall

TESTS := vfio-pci-device-dma-map vfio-pci-huge-fault-race

.DEFAULT_GOAL := all

vfio-pci-device-dma-map: vfio-pci-device-dma-map.o utils.o
	$(CC) -o $@ $^

vfio-pci-huge-fault-race: vfio-pci-huge-fault-race.o utils.o
	$(CC) -o $@ $^

.PHONY: all
all: $(TESTS)

.PHONY: clean
clean:
	rm -f $(TESTS) *.o
