
bin_PROGRAMS =

if WANT_IMG2SIXEL
bin_PROGRAMS += img2sixel
img2sixel_SOURCES = img2sixel.c quant.c stb_image.c
img2sixel_LDFLAGS = -lsixel -L$(top_builddir)/src
img2sixel_CFLAGS = -DSTBI_NO_HDR -I$(top_builddir)/include/
endif
# -DSTBI_SIMD

if WANT_SIXEL2IMG
bin_PROGRAMS += sixel2png
sixel2png_SOURCES = sixel2png.c stb_image_write.c
sixel2png_LDFLAGS = -lsixel -L$(top_builddir)/src
sixel2png_CFLAGS = -I$(top_builddir)/include/
endif

test: all
if WANT_IMG2SIXEL
	./img2sixel ../images/snake.jpg
	./img2sixel ../images/snake.jpg > snake.sixel
endif
if WANT_SIXEL2IMG
	./sixel2png < snake.sixel > snake.png
endif
if WANT_IMG2SIXEL
	./img2sixel -p 16 ../images/egret.jpg
	./img2sixel -m ../images/map8.png ../images/egret.jpg
	./img2sixel -m ../images/map16.png ../images/egret.jpg
endif
