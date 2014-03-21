
CC=cc -O3

.PHONY: all install uninstall clean test

all: img2sixel libsixel.so

install: libsixel.so img2sixel
	cp libsixel.so /usr/local/lib/
	cp img2sixel /usr/local/bin/

uninstall:
	rm -f /usr/local/lib/libsixel.so
	rm -f /usr/local/bin/img2sixel

install-bin: img2sixel

clean:
	rm -f *.o *.so img2sixel

test: img2sixel
	./img2sixel -p 256 images/snake.jpg

libsixel.so: tosixel.o fromsixel.o
	$(CC) -shared $^ -o $@

img2sixel: img2sixel.o quant.o stb_image.o libsixel.so
	$(CC) img2sixel.o quant.o stb_image.o -o $@ -lsixel -L$(PWD)

tosixel.o: tosixel.c
	$(CC) -c $< -o $@

fromsixel.o: fromsixel.c
	$(CC) -c $< -o $@

stb_image.o: stb_image.c
	$(CC) -c $< -o $@

quant.o: quant.c
	$(CC) -c $< -o $@

img2sixel.o: img2sixel.c
	$(CC) -c $< -o $@

