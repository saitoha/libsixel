
CC=cc

all: img2sixel

clean:
	rm -f *.o img2sixel

run: all
	./img2sixel -p 16 a.jpg

img2sixel: tosixel.o main.o
	$(CC) $^ -o $@

tosixel.o: tosixel.c
	$(CC) -c $< -o $@

main.o: main.c
	$(CC) -c $< -o $@
