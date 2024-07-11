CC = gcc
CFLAGS = -O3 -Wall -pedantic -std=c17 -fPIC
H264LIB = h264bitstream/.builddir/libh264bitstream.a

bin/libh264.so: bin/h264.o | bin
	$(CC) -shared -o $@ $^ $(CFLAGS)

bin/h264.o: lib/h264.c lib/h264.h | bin
	$(CC) -c -o $@ $< $(CFLAGS)

clean:
	rm -r bin

bin:
	mkdir -p $@