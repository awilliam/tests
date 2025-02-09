CFLAGS = -g -Wall

TESTS = vfio-pci-device-dma-map

vfio-pci-device-dma-map: vfio-pci-device-dma-map.o utils.o
	$(CC) -o $@ $^

all: $(TESTS)

clean:
	rm -f  $(TESTS) *.o
