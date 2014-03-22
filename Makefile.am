lib_LTLIBRARIES = libsixel.la
libsixel_la_SOURCES = image.c fromsixel.c tosixel.c
libsixel_la_includedir = .
libsixel_la_include_HEADERS = sixel.h config.h

bin_PROGRAMS = img2sixel sixel2png

img2sixel_SOURCES = img2sixel.c quant.c stb_image.c
img2sixel_LDFLAGS = -lsixel -lm
img2sixel_includedir = .
img2sixel_include_HEADERS = sixel.h config.h

sixel2png_SOURCES = sixel2png.c
sixel2png_LDFLAGS = -lsixel
sixel2png_includedir = .
sixel2png_include_HEADERS = sixel.h config.h stb_image_write.h
ACLOCAL_AMFLAGS = -I m4 ${ACLOCAL_FLAGS}

test:
	./img2sixel images/snake.jpg
	./img2sixel images/snake.jpg > snake.sixel
	./sixel2png < snake.sixel > snake.png
