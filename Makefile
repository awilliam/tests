CFLAGS = -g -Wall
SHARED_SRCS = utils.c
HEADERS = utils.h
TEST_SRCS = \
	kvm-huge-guest-test.c \
	leaktest-legacy-kvm.c \
	vfio-correctness-tests.c \
	vfio-huge-guest-test.c \
	vfio-iommu-map-unmap.c \
	vfio-iommu-stress-test.c \
	vfio-noiommu-pci-device-open.c \
	vfio-pci-device-open.c \
	vfio-pci-device-open-igd.c \
	vfio-pci-device-open-sparse-mmap.c \
	vfio-pci-hot-reset.c  \
	vfio-pci-device-dma-map.c \
	vfio-pci-huge-fault-race.c \


SHARED_OBJS = $(SHARED_SRCS:.c=.o)
TEST_BINS = $(TEST_SRCS:.c=)
ARCHIVE_BASE_NAME = vfio-tests
GIT_SHA := $(shell git rev-parse --short HEAD 2>/dev/null)
GIT_DIRTY := $(shell git diff --quiet 2>/dev/null || echo "-dirty")
ifeq ($(GIT_SHA),)
  GIT_VERSION = nogit
else
  GIT_VERSION = $(GIT_SHA)$(GIT_DIRTY)
endif
ARCHIVE_NAME = $(ARCHIVE_BASE_NAME)-$(GIT_VERSION)

.PHONY: all clean archive

all: $(TEST_BINS)

$(TEST_BINS): %: %.o $(SHARED_OBJS)
	$(CC) $(CFLAGS) -o $@ $^

%.o: %.c $(HEADERS) Makefile
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(SHARED_OBJS) $(TEST_SRCS:.c=.o) $(TEST_BINS) $(ARCHIVE_BASE_NAME)*.tar.gz

archive:
	tar -czvf $(ARCHIVE_NAME).tar.gz Makefile $(SHARED_SRCS) $(TEST_SRCS) $(HEADERS)

.PRECIOUS: $(TEST_BINS)
