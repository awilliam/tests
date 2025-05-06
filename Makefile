CFLAGS = -g -Wall
SHARED_SRCS = utils.c
HEADERS = utils.h
TEST_SRCS = vfio-pci-device-dma-map.c vfio-pci-huge-fault-race.c

SHARED_OBJS = $(SHARED_SRCS:.c=.o)
TEST_BINS = $(TEST_SRCS:.c=)
ARCHIVE_NAME = vfio-tests

.PHONY: all clean archive

all: $(TEST_BINS)

$(TEST_BINS): %: %.o $(SHARED_OBJS)
	$(CC) $(CFLAGS) -o $@ $^

%.o: %.c $(HEADERS) Makefile
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(SHARED_OBJS) $(TEST_SRCS:.c=.o) $(TEST_BINS) *.o $(ARCHIVE_NAME).tar.gz

archive:
	tar -czvf $(ARCHIVE_NAME).tar.gz Makefile $(SHARED_SRCS) $(TEST_SRCS) $(HEADERS)

.PRECIOUS: $(TEST_BINS)
