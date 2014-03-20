
CC=cc

all: img2sixel

clean:
	rm -f *.o img2sixel

img2sixel: stb_image.o tosixel.o main.o
	$(CC) $^ -o $@

stb_image.o: stb_image.c
	$(CC) -c $< -o $@

tosixel.o: tosixel.c
	$(CC) -c $< -o $@

main.o: main.c
	$(CC) -c $< -o $@
