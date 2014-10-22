DIST_SUBDIRS = $(SUBDIRS)

bin_PROGRAMS =
dist_man_MANS =

bashcompletiondir = @bashcompletiondir@
dist_bashcompletion_DATA =

zshcompletiondir = @zshcompletiondir@
dist_zshcompletion_DATA =

if COND_GCOV
MAYBE_COVERAGE = --coverage
endif

CLEANFILES = *.gcno *.gcda *.gcov snake1.png snake2.png snake2.sixel

if WANT_IMG2SIXEL
bin_PROGRAMS += img2sixel
img2sixel_SOURCES = img2sixel.c scale.c malloc_stub.c loader.c frompnm.c \
                    scale.h malloc_stub.h loader.h frompnm.h
img2sixel_CFLAGS = -I$(top_builddir)/include/ \
				   $(MAYBE_COVERAGE) $(GDK_PIXBUF_CFLAGS) $(LIBCURL_CFLAGS) \
				   $(GD_CFLAGS) $(JPEG_CFLAGS) $(LIBPNG_CFLAGS)
if COND_DEBUG
img2sixel_CFLAGS += -Werror
endif
img2sixel_LDADD = $(top_builddir)/src/libsixel.la -lm \
				  $(GDK_PIXBUF_LIBS) $(LIBCURL_LIBS) $(GD_LIBS) $(JPEG_LIBS) $(LIBPNG_LIBS)
dist_man_MANS += img2sixel.1
dist_bashcompletion_DATA += shell-completion/bash/img2sixel
dist_zshcompletion_DATA += shell-completion/zsh/_img2sixel
endif

if WANT_SIXEL2PNG
bin_PROGRAMS += sixel2png
sixel2png_SOURCES = sixel2png.c stb_image_write.c malloc_stub.c \
                    stb_image_write.h malloc_stub.h
sixel2png_CFLAGS = -Wall -I$(top_builddir)/include/ $(MAYBE_COVERAGE) $(LIBPNG_CFLAGS)
if COND_DEBUG
sixel2png_CFLAGS += -Werror
endif
sixel2png_LDADD = $(top_builddir)/src/libsixel.la $(LIBPNG_LIBS)
dist_man_MANS += sixel2png.1
endif

test: all
if WANT_IMG2SIXEL
	$(WINE) ./img2sixel ../images/snake.jpg | tee snake.sixel
	$(WINE) ./img2sixel ../images/snake.jpg -w50% --height=100 | tee snake2.sixel
if HAVE_JPEG
	$(WINE) ./img2sixel ../images/snake-progressive.jpg | tee snake.sixel
endif
if HAVE_PNG
	$(WINE) ./img2sixel ../images/snake.png | tee snake.sixel
endif
endif
if WANT_SIXEL2PNG
	$(WINE) ./sixel2png < snake.sixel > snake1.png
	$(WINE) ./sixel2png -i snake.sixel -o snake2.png
endif
if WANT_IMG2SIXEL
	$(WINE) ./img2sixel -m ../images/map8.png ../images/egret.jpg
	$(WINE) ./img2sixel -m ../images/map16.png ../images/snake.pnm
	$(WINE) ./img2sixel -p 8 ../images/snake.jpg
	$(WINE) ./img2sixel -p 2 ../images/snake.jpg
	$(WINE) ./img2sixel -e ../images/snake.jpg
endif

winetest: all
	WINE=wine $(MAKE) test
