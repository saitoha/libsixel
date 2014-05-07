
bin_PROGRAMS =
dist_man_MANS =

if WANT_IMG2SIXEL
bin_PROGRAMS += img2sixel
img2sixel_SOURCES = img2sixel.c quant.c scale.c stb_image.c malloc_stub.c
img2sixel_LDADD = $(top_builddir)/src/libsixel.la -lm
img2sixel_CFLAGS = -O3 -I$(top_builddir)/include/
dist_man_MANS += img2sixel.1
endif
# -DSTBI_SIMD

if WANT_SIXEL2PNG
bin_PROGRAMS += sixel2png
sixel2png_SOURCES = sixel2png.c stb_image_write.c malloc_stub.c
sixel2png_LDADD = $(top_builddir)/src/libsixel.la
sixel2png_CFLAGS = -I$(top_builddir)/include/
dist_man_MANS += sixel2png.1
endif

test: all
if WANT_IMG2SIXEL
	./img2sixel ../images/snake.jpg
	./img2sixel ../images/snake.jpg > snake.sixel
endif
if WANT_SIXEL2PNG
	./sixel2png < snake.sixel > snake.png
endif
if WANT_IMG2SIXEL
	./img2sixel -p 16 ../images/egret.jpg
	./img2sixel -m ../images/map8.png ../images/egret.jpg
	./img2sixel -m ../images/map16.png ../images/egret.jpg
endif

winetest: all
if WANT_IMG2SIXEL
	wine ./img2sixel.exe ../images/snake.jpg
	wine ./img2sixel.exe ../images/snake.jpg > snake.sixel
endif
if WANT_SIXEL2PNG
	wine ./sixel2png.exe < snake.sixel > snake.png
endif
if WANT_IMG2SIXEL
	wine ./img2sixel -p 16 ../images/egret.jpg
	wine ./img2sixel -m ../images/map8.png ../images/egret.jpg
	wine ./img2sixel -m ../images/map16.png ../images/egret.jpg
endif
