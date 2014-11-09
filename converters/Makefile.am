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

CLEANFILES = *.gcno *.gcda *.gcov *.png *.sixel

if WANT_IMG2SIXEL
bin_PROGRAMS += img2sixel
img2sixel_SOURCES = img2sixel.c scale.c malloc_stub.c loader.c frompnm.c \
                    scale.h malloc_stub.h loader.h frompnm.h
img2sixel_CFLAGS = -I$(top_builddir)/include/ \
				   $(MAYBE_COVERAGE) $(GDK_PIXBUF_CFLAGS) $(LIBCURL_CFLAGS) \
				   $(GD_CFLAGS) $(LIBJPEG_CFLAGS) $(LIBPNG_CFLAGS)
if COND_DEBUG
img2sixel_CFLAGS += -Werror
endif
img2sixel_LDADD = $(top_builddir)/src/libsixel.la -lm \
				  $(GDK_PIXBUF_LIBS) $(LIBCURL_LIBS) $(GD_LIBS) \
				  $(LIBJPEG_LIBS) $(LIBPNG_LIBS)
dist_man_MANS += img2sixel.1
dist_bashcompletion_DATA += shell-completion/bash/img2sixel
dist_zshcompletion_DATA += shell-completion/zsh/_img2sixel
endif

if WANT_SIXEL2PNG
bin_PROGRAMS += sixel2png
sixel2png_SOURCES = sixel2png.c stb_image_write.c malloc_stub.c \
                    stb_image_write.h malloc_stub.h
sixel2png_CFLAGS = -Wall -I$(top_builddir)/include/ \
				   $(MAYBE_COVERAGE) $(LIBPNG_CFLAGS)
if COND_DEBUG
sixel2png_CFLAGS += -Werror
endif
sixel2png_LDADD = $(top_builddir)/src/libsixel.la $(LIBPNG_LIBS)
dist_man_MANS += sixel2png.1
endif

test: all
if WANT_IMG2SIXEL
	! $(WINE) ./img2sixel -d invalid_option
	! $(WINE) ./img2sixel -r invalid_option
	! $(WINE) ./img2sixel -s invalid_option
	! $(WINE) ./img2sixel -t invalid_option
	! $(WINE) ./img2sixel -w invalid_option
	! $(WINE) ./img2sixel -h invalid_option
	! $(WINE) ./img2sixel -f invalid_option
	! $(WINE) ./img2sixel -q invalid_option
	! $(WINE) ./img2sixel -l invalid_option
	! $(WINE) ./img2sixel -%
	! $(WINE) ./img2sixel invalid_filename
	! $(WINE) ./img2sixel -m invalid_filename ../images/snake.jpg
	! $(WINE) ./img2sixel -m ../images/map8.png -p 8 ../images/snake.jpg
	! $(WINE) ./img2sixel -m ../images/map8.png -e ../images/snake.jpg
	! $(WINE) ./img2sixel -p16 -e ../images/snake.jpg
	! $(WINE) ./img2sixel -D ../images/snake.jpg
	! $(WINE) ./img2sixel -I -C0 ../images/snake.png
	$(WINE) ./img2sixel -H
	$(WINE) ./img2sixel -V
	$(WINE) ./img2sixel ../images/snake.jpg -datkinson -flum -saverage | tee snake.sixel
	$(WINE) ./img2sixel -w50% -h150% -dfs -thls < ../images/snake.jpg -shistogram | tee snake2.sixel
	$(WINE) ./img2sixel -w2100 -h2100 < ../images/snake.jpg > snake3.sixel
	$(WINE) ./img2sixel - -w105% -h100 -djajuni -rnearest < ../images/snake.gif
	$(WINE) ./img2sixel ../images/snake.tga -7 -sauto -w100 -rgaussian  -dburkes -tauto
	$(WINE) ./img2sixel -p300 ../images/snake.tiff -8 -scenter -h100 -qauto -rhanning -dstucki -thls
	$(WINE) ./img2sixel -8 -qauto -thls -e ../images/snake.pgm
	$(WINE) ./img2sixel -7 -w100 -h100 ../images/snake.pbm
	$(WINE) ./img2sixel -I ../images/snake.ppm -dstucki -thls
	$(WINE) ./img2sixel -I ../images/snake-ascii.ppm -8 -dburkes
	$(WINE) ./img2sixel -I -C10 ../images/snake.png -djajuni
	$(WINE) ./img2sixel -I ../images/snake-ascii.pgm
	$(WINE) ./img2sixel -I ../images/snake-ascii.pbm -datkinson
	$(WINE) ./img2sixel -I  ../images/snake.ppm -c2000x100+40+20 -wauto -h200 -qhigh -dfs -rbilinear -trgb
	$(WINE) ./img2sixel -I -v ../images/snake.bmp -w200 -hauto -c100x1000+40+20 -qlow -dnone -rhamming -thls
	$(WINE) ./img2sixel -m ../images/map8.png -w200 -fauto -rwelsh ../images/egret.jpg
	$(WINE) ./img2sixel -m ../images/map16.png -w100 -hauto -rbicubic -dauto ../images/snake.ppm
	$(WINE) ./img2sixel -p 16 -C3 ../images/snake.jpg -h100 -fnorm -rlanczos2
	$(WINE) ./img2sixel -v -p 8 ../images/snake.jpg -h200 -fnorm -rlanczos2 -dnone
	$(WINE) ./img2sixel -p 2 ../images/snake.jpg -h100 -wauto -rlanczos3
	$(WINE) ./img2sixel -p 1 ../images/snake.jpg -h100 -n1
	$(WINE) ./img2sixel -e ../images/snake.jpg -h140 -rlanczos4 -P
	$(WINE) ./img2sixel -e -i ../images/snake.jpg -P > /dev/null
	$(WINE) ./img2sixel ../images/seq2gif.gif -ldisable -dnone -u
	$(WINE) ./img2sixel ../images/seq2gif.gif -ldisable -dnone -g
	$(WINE) ./img2sixel ../images/seq2gif.gif -S -datkinson
if HAVE_JPEG
	$(WINE) ./img2sixel ../images/snake-progressive.jpg | tee snake.sixel
endif
if HAVE_PNG
	$(WINE) ./img2sixel ../images/snake.png | tee snake.sixel
endif
if HAVE_CURL
	$(WINE) ./img2sixel file:///$$(pwd)/../images/snake.jpg
endif
if WANT_SIXEL2PNG
	! $(WINE) ./sixel2png -i unknown.sixel
	! $(WINE) ./sixel2png -% < snake.sixel
	$(WINE) ./sixel2png -H
	$(WINE) ./sixel2png -V
	$(WINE) ./sixel2png < snake.sixel > snake1.png
	$(WINE) ./sixel2png < snake2.sixel > snake2.png
	$(WINE) ./sixel2png - - < snake3.sixel > snake3.png
	$(WINE) ./sixel2png --input=snake.sixel -o snake4.png
endif
endif
	@echo succeeded

winetest: all
	WINE=wine $(MAKE) test
