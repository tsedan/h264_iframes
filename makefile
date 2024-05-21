CC = g++
CFLAGS = -O3 -Wall -pedantic -std=c++17 -fPIC

bin/libh264.so: bin/h264.o | bin
	$(CC) -shared -o $@ $^ $(CFLAGS)

bin/h264.o: lib/h264.cpp lib/h264.h | bin
	$(CC) -c -o $@ $< $(CFLAGS)

clean:
	rm -rf bin

bin:
	mkdir -p $@