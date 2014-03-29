include_HEADERS = sixel.h stb_image_write.h

lib_LTLIBRARIES = libsixel.la
libsixel_la_SOURCES = image.c output.c fromsixel.c tosixel.c

bin_PROGRAMS = img2sixel sixel2png
img2sixel_SOURCES = img2sixel.c quant.c stb_image.c
img2sixel_LDFLAGS = -lsixel
img2sixel_CFLAGS = -DSTBI_NO_HDR #-DSTBI_SIMD

sixel2png_SOURCES = sixel2png.c stb_image_write.c
sixel2png_LDFLAGS = -lsixel

ACLOCAL_AMFLAGS = -I m4 ${ACLOCAL_FLAGS}

test:
	./img2sixel ../images/snake.jpg
	./img2sixel ../images/snake.jpg > snake.sixel
	./sixel2png < snake.sixel > snake.png
	./img2sixel -p 16 ../images/egret.jpg
	./img2sixel -m ../images/map8.png ../images/egret.jpg
	./img2sixel -m ../images/map16.png ../images/egret.jpg
