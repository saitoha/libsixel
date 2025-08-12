all:
	cc -Wall $$(libsixel-config --cflags) main.c -o demo $$(libsixel-config --libs)

run: all
	./demo

run-dec: all
	./demo dec

run-sgr: all
	./demo sgr

clean:
	rm -vf ./demo
