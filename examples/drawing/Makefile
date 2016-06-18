all:
	cc -Wall $$(libsixel-config --cflags) main.c -o demo $$(libsixel-config --libs)

run: all
	./demo

clean:
	rm -vf ./demo
