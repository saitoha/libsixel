CC=cc
CFLAGS=-Wall -Wno-deprecated-declarations `pkg-config --cflags libsixel`
LDFLAGS=`pkg-config --libs libsixel` -framework OpenGL

all: opengl.c Makefile
	$(CC) $(CFLAGS) opengl.c $(LDFLAGS) -o opengl

run: all
	./opengl

clean:
	rm -f opengl *.o
