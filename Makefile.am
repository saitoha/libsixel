lib_LTLIBRARIES = libsixel.la
libsixel_la_SOURCES = image.c fromsixel.c tosixel.c

bin_PROGRAMS = img2sixel sixel2png
img2sixel_SOURCES = img2sixel.c quant.c stb_image.c
img2sixel_LDFLAGS = -lsixel
sixel2png_SOURCES = sixel2png.c
sixel2png_LDFLAGS = -lsixel

ACLOCAL_AMFLAGS = -I m4 ${ACLOCAL_FLAGS}
