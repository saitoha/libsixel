DIST_SUBDIRS = $(SUBDIRS)

srcdir = @srcdir@
top_builddir = @top_builddir@
builddir = @builddir@
VPATH = @srcdir@

bin_PROGRAMS =
dist_man_MANS =

bashcompletiondir = @bashcompletiondir@
dist_bashcompletion_DATA =

zshcompletiondir = @zshcompletiondir@
dist_zshcompletion_DATA =

if COND_GCOV
MAYBE_COVERAGE = --coverage
endif

CLEANFILES = *.gcno *.gcda *.gcov \
  $(builddir)/tmp/*.png \
  $(builddir)/tmp/*.sixel \
  $(builddir)/tmp/*.txt \
  $(builddir)/tmp/*.pipe \
  $(builddir)/tmp/server.py \
  $(builddir)/tmp/server.key \
  $(builddir)/tmp/server.crt \
  $(builddir)/tmp/testfile

if WANT_IMG2SIXEL
bin_PROGRAMS += img2sixel
img2sixel_SOURCES = $(srcdir)/img2sixel.c $(srcdir)/malloc_stub.c $(srcdir)/malloc_stub.h
img2sixel_CPPFLAGS = -I$(top_builddir)/include/ $(AM_CPPFLAGS)
img2sixel_CFLAGS = $(CFLAGS) $(AM_CFLAGS) $(MAYBE_COVERAGE)
img2sixel_LDADD = $(top_builddir)/src/libsixel.la
dist_man_MANS += $(srcdir)/img2sixel.1
dist_bashcompletion_DATA += $(srcdir)/shell-completion/bash/img2sixel
dist_zshcompletion_DATA += $(srcdir)/shell-completion/zsh/_img2sixel
endif

if WANT_SIXEL2PNG
bin_PROGRAMS += sixel2png
sixel2png_SOURCES = $(srcdir)/sixel2png.c $(srcdir)/malloc_stub.c $(srcdir)/malloc_stub.h
sixel2png_CPPFLAGS = -I$(top_builddir)/include/ $(AM_CPPFLAGS)
sixel2png_CFLAGS = $(CFLAGS) $(AM_CFLAGS) $(MAYBE_COVERAGE)
sixel2png_LDADD = $(top_builddir)/src/libsixel.la
dist_man_MANS += $(srcdir)/sixel2png.1
endif

check: test_impl

test: all test_impl

test_impl:
	@echo '[start]'

if WANT_IMG2SIXEL

	@echo '[test1] invalid option handling'
	mkdir -p $(builddir)/tmp/
	touch $(builddir)/tmp/testfile
	chmod -r $(builddir)/tmp/testfile
	test ! $$($(WINE) $(builddir)/img2sixel$(WINEEXT) $(builddir)/tmp/testfile)
	rm -f $(builddir)/tmp/testfile
	test ! $$($(WINE) $(builddir)/img2sixel$(WINEEXT) $(builddir)/tmp/invalid_filename)
	test ! $$($(WINE) $(builddir)/img2sixel$(WINEEXT) .)
	test ! $$($(WINE) $(builddir)/img2sixel$(WINEEXT) -d invalid_option)
	test ! $$($(WINE) $(builddir)/img2sixel$(WINEEXT) -r invalid_option)
	test ! $$($(WINE) $(builddir)/img2sixel$(WINEEXT) -s invalid_option)
	test ! $$($(WINE) $(builddir)/img2sixel$(WINEEXT) -t invalid_option)
	test ! $$($(WINE) $(builddir)/img2sixel$(WINEEXT) -w invalid_option)
	test ! $$($(WINE) $(builddir)/img2sixel$(WINEEXT) -h invalid_option)
	test ! $$($(WINE) $(builddir)/img2sixel$(WINEEXT) -f invalid_option)
	test ! $$($(WINE) $(builddir)/img2sixel$(WINEEXT) -q invalid_option)
	test ! $$($(WINE) $(builddir)/img2sixel$(WINEEXT) -l invalid_option)
	test ! $$($(WINE) $(builddir)/img2sixel$(WINEEXT) -b invalid_option)
	test ! $$($(WINE) $(builddir)/img2sixel$(WINEEXT) -E invalid_option)
	test ! $$($(WINE) $(builddir)/img2sixel$(WINEEXT) -B invalid_option)
	test ! $$($(WINE) $(builddir)/img2sixel$(WINEEXT) -B \#ffff $(top_srcdir)/images/map8.png)
	test ! $$($(WINE) $(builddir)/img2sixel$(WINEEXT) -B \#0000000000000 $(top_srcdir)/images/map8.png)
	test ! $$($(WINE) $(builddir)/img2sixel$(WINEEXT) -B \#00G)
	test ! $$($(WINE) $(builddir)/img2sixel$(WINEEXT) -B test)
	test ! $$($(WINE) $(builddir)/img2sixel$(WINEEXT) -B rgb:11/11)
	test ! $$($(WINE) $(builddir)/img2sixel$(WINEEXT) -%)
	test ! $$($(WINE) $(builddir)/img2sixel$(WINEEXT) -m $(builddir)/tmp/invalid_filename $(top_srcdir)/images/snake.jpg)
	test ! $$($(WINE) $(builddir)/img2sixel$(WINEEXT) -p16 -e $(top_srcdir)/images/snake.jpg)
	test ! $$($(WINE) $(builddir)/img2sixel$(WINEEXT) -I -C0 $(top_srcdir)/images/snake.png)
	test ! $$($(WINE) $(builddir)/img2sixel$(WINEEXT) -I -p8 $(top_srcdir)/images/snake.png)
	test ! $$($(WINE) $(builddir)/img2sixel$(WINEEXT) -p64 -bxterm256 $(top_srcdir)/images/snake.png)
	test ! $$($(WINE) $(builddir)/img2sixel$(WINEEXT) -8 -P $(top_srcdir)/images/snake.png)

	@echo '[test2] STDIN handling'
	test ! $$(echo -n a | $(WINE) $(builddir)/img2sixel$(WINEEXT))

	@echo '[test3] print information'
	$(WINE) $(builddir)/img2sixel$(WINEEXT) -H
	$(WINE) $(builddir)/img2sixel$(WINEEXT) -V

	@echo '[test4] conversion options'
	$(WINE) $(builddir)/img2sixel$(WINEEXT) $(srcdir)/images/snake.jpg -datkinson -flum -saverage | $(WINE) $(builddir)/img2sixel$(WINEEXT) | tee $(builddir)/tmp/snake.sixel
	$(WINE) $(builddir)/img2sixel$(WINEEXT) -w50% -h150% -dfs -Bblue -thls -shistogram < $(top_srcdir)/images/snake.jpg | tee $(builddir)/tmp/snake2.sixel
	$(WINE) printf '\033Pq"1;1;1;1!6~\033\\' | $(builddir)/img2sixel$(WINEEXT) -r nearest -w200% | tr \# \\n | tail -n+3 | od -An -tx1 | tr -d \  | xargs test 302131327e2d2131327e1b5c =
	$(WINE) printf '\033Pq"1;1;1;1!6~\033\\' | $(builddir)/img2sixel$(WINEEXT) -r nearest -h200% | tr \# \\n | tail -n+3 | od -An -tx1 | tr -d \  | xargs test 302131327e2d2131327e1b5c =
	$(WINE) printf '\033Pq"1;1;1;1!6~\033\\' | $(builddir)/img2sixel$(WINEEXT) -r nearest -h200% -wauto | tr \# \\n | tail -n+3 | od -An -tx1 | tr -d \  | xargs test 302131327e2d2131327e1b5c =
	$(WINE) printf '\033Pq"1;1;1;1!6~\033\\' | $(builddir)/img2sixel$(WINEEXT) -r nearest -hauto -w12 | tr \# \\n | tail -n+3 | od -An -tx1 | tr -d \  | xargs test 302131327e2d2131327e1b5c =
	$(WINE) printf '\033Pq"1;1;1;1!6~\033\\' | $(builddir)/img2sixel$(WINEEXT) -r nearest -h12 -w200% | tr \# \\n | tail -n+3 | od -An -tx1 | tr -d \  | xargs test 302131327e2d2131327e1b5c =
	$(WINE) $(builddir)/img2sixel$(WINEEXT) -w210 -h210 -djajuni -bxterm256 -o $(builddir)/tmp/snake3.sixel < $(top_srcdir)/images/snake.jpg
	$(WINE) $(builddir)/img2sixel$(WINEEXT) --height=100 --diffusion=atkinson --outfile=$(builddir)/tmp/snake4.sixel < $(top_srcdir)/images/snake.jpg
	$(WINE) $(builddir)/img2sixel$(WINEEXT) -w105% -h100 -B\#000000000 -rnearest < $(top_srcdir)/images/snake.gif
	$(WINE) $(builddir)/img2sixel$(WINEEXT) -7 -sauto -w100 -rgaussian -qauto -dburkes -tauto $(top_srcdir)/images/snake.tga
	$(WINE) $(builddir)/img2sixel$(WINEEXT) -p200 -8 -scenter -Brgb:0/f/A -h100 -qfull -rhanning -dstucki -thls $(top_srcdir)/images/snake.tiff
	$(WINE) $(builddir)/img2sixel$(WINEEXT) -8 -qauto -thls -e $(top_srcdir)/images/snake.pgm
	$(WINE) $(builddir)/img2sixel$(WINEEXT) -8 -m $(top_srcdir)/images/map8-palette.png -Esize $(top_srcdir)/images/snake.ppm
	$(WINE) $(builddir)/img2sixel$(WINEEXT) -7 -m $(top_srcdir)/images/map16-palette.png -Efast $(top_srcdir)/images/snake.jpg
	$(WINE) $(builddir)/img2sixel$(WINEEXT) -7 -w300 $(top_srcdir)/images/snake-palette.png
	$(WINE) $(builddir)/img2sixel$(WINEEXT) -7 -w100 -h100 -bxterm16 -B\#aB3 -B\#aB3 $(top_srcdir)/images/snake.pbm
	$(WINE) $(builddir)/img2sixel$(WINEEXT) -I -dstucki -thls -B\#a0B030 $(top_srcdir)/images/snake.ppm
	$(WINE) $(builddir)/img2sixel$(WINEEXT) -bvt340color $(top_srcdir)/images/snake.ppm
	$(WINE) $(builddir)/img2sixel$(WINEEXT) -bvt340mono $(top_srcdir)/images/snake.tga
	$(WINE) $(builddir)/img2sixel$(WINEEXT) -bgray1 -w120 $(top_srcdir)/images/snake.tga
	$(WINE) $(builddir)/img2sixel$(WINEEXT) -bgray2 -w120 $(top_srcdir)/images/snake.tga
	$(WINE) $(builddir)/img2sixel$(WINEEXT) -bgray4 -w120 $(top_srcdir)/images/snake.tga
	$(WINE) $(builddir)/img2sixel$(WINEEXT) -bgray8 -w120 $(top_srcdir)/images/snake.tga
	$(WINE) $(builddir)/img2sixel$(WINEEXT) -I -8 -dburkes -B\#ffffffffffff $(top_srcdir)/images/snake-ascii.ppm
	$(WINE) $(builddir)/img2sixel$(WINEEXT) -I -C10 -djajuni $(top_srcdir)/images/snake.png
	$(WINE) $(builddir)/img2sixel$(WINEEXT) -I -Eauto $(top_srcdir)/images/snake-ascii.pgm
	$(WINE) $(builddir)/img2sixel$(WINEEXT) -I -datkinson $(top_srcdir)/images/snake-ascii.pbm
	$(WINE) $(builddir)/img2sixel$(WINEEXT) $(top_srcdir)/images/snake-grayscale.png
	$(WINE) $(builddir)/img2sixel$(WINEEXT) -m $(top_srcdir)/images/map8-palette.png $(top_srcdir)/images/snake-grayscale.png
	$(WINE) $(builddir)/img2sixel$(WINEEXT) -m $(top_srcdir)/images/snake-grayscale.png $(top_srcdir)/images/snake.png
	$(WINE) $(builddir)/img2sixel$(WINEEXT) -c200x200+100+100 -dx_dither $(top_srcdir)/images/snake-grayscale.png
	$(WINE) $(builddir)/img2sixel$(WINEEXT) -c200x200+100+100 -w400 -da_dither $(top_srcdir)/images/snake-grayscale.png
	$(WINE) $(builddir)/img2sixel$(WINEEXT) -I $(top_srcdir)/images/snake-grayscale.png
	$(WINE) $(builddir)/img2sixel$(WINEEXT) -I $(top_srcdir)/images/snake-grayscale.jpg
	$(WINE) $(builddir)/img2sixel$(WINEEXT) -m $(top_srcdir)/images/map8.six -m $(top_srcdir)/images/map8.six $(top_srcdir)/images/snake.six
	$(WINE) $(builddir)/img2sixel$(WINEEXT) -w200 -p8 $(top_srcdir)/images/snake.six
	$(WINE) $(builddir)/img2sixel$(WINEEXT) -c200x200+2000+2000 $(top_srcdir)/images/snake.six
	$(WINE) $(builddir)/img2sixel$(WINEEXT) -bxterm16 $(top_srcdir)/images/snake.six
	$(WINE) $(builddir)/img2sixel$(WINEEXT) -e $(top_srcdir)/images/snake.six
	$(WINE) $(builddir)/img2sixel$(WINEEXT) -I $(top_srcdir)/images/snake.six
	$(WINE) $(builddir)/img2sixel$(WINEEXT) -I -da_dither -w100 $(top_srcdir)/images/snake.six
	$(WINE) $(builddir)/img2sixel$(WINEEXT) -I -dx_dither -h100 $(top_srcdir)/images/snake.six
	$(WINE) $(builddir)/img2sixel$(WINEEXT) -I -c2000x100+40+20 -wauto -h200 -qhigh -dfs -rbilinear -trgb $(top_srcdir)/images/snake.ppm
	$(WINE) $(builddir)/img2sixel$(WINEEXT) -I -v -w200 -hauto -c100x1000+40+20 -qlow -dnone -rhamming -thls $(top_srcdir)/images/snake.bmp
	$(WINE) $(builddir)/img2sixel$(WINEEXT) -m $(top_srcdir)/images/map8.png -w200 -fauto -rwelsh $(top_srcdir)/images/egret.jpg
	$(WINE) $(builddir)/img2sixel$(WINEEXT) -m $(top_srcdir)/images/map16.png -w100 -hauto -rbicubic -dauto $(top_srcdir)/images/snake.ppm
	$(WINE) $(builddir)/img2sixel$(WINEEXT) -p 16 -C3 -h100 -fnorm -rlanczos2 $(top_srcdir)/images/snake.jpg
	$(WINE) $(builddir)/img2sixel$(WINEEXT) -v -p 8 -h200 -fnorm -rlanczos2 -dnone $(top_srcdir)/images/snake.jpg
	$(WINE) $(builddir)/img2sixel$(WINEEXT) -p 2 -h100 -wauto -rlanczos3 $(top_srcdir)/images/snake.jpg
	$(WINE) $(builddir)/img2sixel$(WINEEXT) -p 1 -h100 -n1 $(top_srcdir)/images/snake.jpg && printf '\033[*1z'
	$(WINE) $(builddir)/img2sixel$(WINEEXT) -e -h140 -rlanczos4 -P $(top_srcdir)/images/snake.jpg
	$(WINE) $(builddir)/img2sixel$(WINEEXT) -e -i -P $(top_srcdir)/images/snake.jpg > /dev/null
	$(WINE) $(builddir)/img2sixel$(WINEEXT) -w204 -h204 $(top_srcdir)/images/snake.png | $(WINE) $(builddir)/img2sixel$(WINEEXT) > /dev/null

	@echo '[test5] DCS arguments handling'
	seq 0 10 | while read i; do \
	    seq 0 2 | while read j; do \
	            $(WINE) $(builddir)/img2sixel$(WINEEXT) $(top_srcdir)/images/map8.png | \
	            sed "s/Pq/P$${i};;$${j}q/" | \
	            $(WINE) $(builddir)/img2sixel$(WINEEXT); \
	    done; \
	done

	@echo
	@echo '[test6] DCS format variations'
	$(WINE) $(builddir)/img2sixel$(WINEEXT) $(top_srcdir)/images/snake.png| sed 's/C/C:/g'| tr : '\t'| $(WINE) $(builddir)/img2sixel$(WINEEXT)
	$(WINE) $(builddir)/img2sixel$(WINEEXT) $(top_srcdir)/images/snake.png| sed 's/"1;1;600;450/"1;1;700;500/'| $(WINE) $(builddir)/img2sixel$(WINEEXT)

	@echo
	@echo '[test7] animation'
	$(WINE) $(builddir)/img2sixel$(WINEEXT) -ldisable -dnone -u -lauto $(top_srcdir)/images/seq2gif.gif
	$(WINE) $(builddir)/img2sixel$(WINEEXT) -ldisable -dnone -g $(top_srcdir)/images/seq2gif.gif
	$(WINE) $(builddir)/img2sixel$(WINEEXT) -ldisable -dnone -u -g $(top_srcdir)/images/seq2gif.gif
	$(WINE) $(builddir)/img2sixel$(WINEEXT) -S -datkinson $(top_srcdir)/images/seq2gif.gif

	@echo
	@echo '[test8] progressive jpeg'
	$(WINE) $(builddir)/img2sixel$(WINEEXT) $(top_srcdir)/images/snake-progressive.jpg
if HAVE_PNG
	@echo
	@echo '[test9] various PNG'
	$(WINE) $(builddir)/img2sixel$(WINEEXT) $(top_srcdir)/images/pngsuite/basic/basn0g01.png
	$(WINE) $(builddir)/img2sixel$(WINEEXT) $(top_srcdir)/images/pngsuite/basic/basn0g02.png
	$(WINE) $(builddir)/img2sixel$(WINEEXT) $(top_srcdir)/images/pngsuite/basic/basn0g04.png
	$(WINE) $(builddir)/img2sixel$(WINEEXT) $(top_srcdir)/images/pngsuite/basic/basn0g08.png
	$(WINE) $(builddir)/img2sixel$(WINEEXT) $(top_srcdir)/images/pngsuite/basic/basn0g16.png
	$(WINE) $(builddir)/img2sixel$(WINEEXT) $(top_srcdir)/images/pngsuite/basic/basn3p01.png
	$(WINE) $(builddir)/img2sixel$(WINEEXT) $(top_srcdir)/images/pngsuite/basic/basn3p02.png
	$(WINE) $(builddir)/img2sixel$(WINEEXT) $(top_srcdir)/images/pngsuite/basic/basn3p04.png
	$(WINE) $(builddir)/img2sixel$(WINEEXT) $(top_srcdir)/images/pngsuite/basic/basn3p08.png
	$(WINE) $(builddir)/img2sixel$(WINEEXT) $(top_srcdir)/images/pngsuite/basic/basn4a08.png
	$(WINE) $(builddir)/img2sixel$(WINEEXT) $(top_srcdir)/images/pngsuite/basic/basn4a16.png
	$(WINE) $(builddir)/img2sixel$(WINEEXT) $(top_srcdir)/images/pngsuite/basic/basn6a08.png
	$(WINE) $(builddir)/img2sixel$(WINEEXT) $(top_srcdir)/images/pngsuite/basic/basn6a16.png
	$(WINE) $(builddir)/img2sixel$(WINEEXT) -w32 $(top_srcdir)/images/pngsuite/basic/basn0g01.png
	$(WINE) $(builddir)/img2sixel$(WINEEXT) -w32 $(top_srcdir)/images/pngsuite/basic/basn0g02.png
	$(WINE) $(builddir)/img2sixel$(WINEEXT) -w32 $(top_srcdir)/images/pngsuite/basic/basn0g04.png
	$(WINE) $(builddir)/img2sixel$(WINEEXT) -w32 $(top_srcdir)/images/pngsuite/basic/basn0g08.png
	$(WINE) $(builddir)/img2sixel$(WINEEXT) -w32 $(top_srcdir)/images/pngsuite/basic/basn0g16.png
	$(WINE) $(builddir)/img2sixel$(WINEEXT) -w32 $(top_srcdir)/images/pngsuite/basic/basn3p01.png
	$(WINE) $(builddir)/img2sixel$(WINEEXT) -w32 $(top_srcdir)/images/pngsuite/basic/basn3p02.png
	$(WINE) $(builddir)/img2sixel$(WINEEXT) -w32 $(top_srcdir)/images/pngsuite/basic/basn3p04.png
	$(WINE) $(builddir)/img2sixel$(WINEEXT) -w32 $(top_srcdir)/images/pngsuite/basic/basn3p08.png
	$(WINE) $(builddir)/img2sixel$(WINEEXT) -w32 $(top_srcdir)/images/pngsuite/basic/basn4a08.png
	$(WINE) $(builddir)/img2sixel$(WINEEXT) -w32 $(top_srcdir)/images/pngsuite/basic/basn4a16.png
	$(WINE) $(builddir)/img2sixel$(WINEEXT) -w32 $(top_srcdir)/images/pngsuite/basic/basn6a08.png
	$(WINE) $(builddir)/img2sixel$(WINEEXT) -w32 $(top_srcdir)/images/pngsuite/basic/basn6a16.png
	$(WINE) $(builddir)/img2sixel$(WINEEXT) -c16x16+8+8 $(top_srcdir)/images/pngsuite/basic/basn0g01.png
	$(WINE) $(builddir)/img2sixel$(WINEEXT) -c16x16+8+8 $(top_srcdir)/images/pngsuite/basic/basn0g02.png
	$(WINE) $(builddir)/img2sixel$(WINEEXT) -c16x16+8+8 $(top_srcdir)/images/pngsuite/basic/basn0g04.png
	$(WINE) $(builddir)/img2sixel$(WINEEXT) -c16x16+8+8 $(top_srcdir)/images/pngsuite/basic/basn0g08.png
	$(WINE) $(builddir)/img2sixel$(WINEEXT) -c16x16+8+8 $(top_srcdir)/images/pngsuite/basic/basn0g16.png
	$(WINE) $(builddir)/img2sixel$(WINEEXT) -c16x16+8+8 $(top_srcdir)/images/pngsuite/basic/basn3p01.png
	$(WINE) $(builddir)/img2sixel$(WINEEXT) -c16x16+8+8 $(top_srcdir)/images/pngsuite/basic/basn3p02.png
	$(WINE) $(builddir)/img2sixel$(WINEEXT) -c16x16+8+8 $(top_srcdir)/images/pngsuite/basic/basn3p04.png
	$(WINE) $(builddir)/img2sixel$(WINEEXT) -c16x16+8+8 $(top_srcdir)/images/pngsuite/basic/basn3p08.png
	$(WINE) $(builddir)/img2sixel$(WINEEXT) -c16x16+8+8 $(top_srcdir)/images/pngsuite/basic/basn4a08.png
	$(WINE) $(builddir)/img2sixel$(WINEEXT) -c16x16+8+8 $(top_srcdir)/images/pngsuite/basic/basn4a16.png
	$(WINE) $(builddir)/img2sixel$(WINEEXT) -c16x16+8+8 $(top_srcdir)/images/pngsuite/basic/basn6a08.png
	$(WINE) $(builddir)/img2sixel$(WINEEXT) -c16x16+8+8 $(top_srcdir)/images/pngsuite/basic/basn6a16.png
	$(WINE) $(builddir)/img2sixel$(WINEEXT) $(top_srcdir)/images/pngsuite/background/bgai4a08.png
	$(WINE) $(builddir)/img2sixel$(WINEEXT) $(top_srcdir)/images/pngsuite/background/bgai4a16.png
	$(WINE) $(builddir)/img2sixel$(WINEEXT) $(top_srcdir)/images/pngsuite/background/bgan6a08.png
	$(WINE) $(builddir)/img2sixel$(WINEEXT) $(top_srcdir)/images/pngsuite/background/bgan6a16.png
	$(WINE) $(builddir)/img2sixel$(WINEEXT) $(top_srcdir)/images/pngsuite/background/bgbn4a08.png
	$(WINE) $(builddir)/img2sixel$(WINEEXT) $(top_srcdir)/images/pngsuite/background/bggn4a16.png
	$(WINE) $(builddir)/img2sixel$(WINEEXT) $(top_srcdir)/images/pngsuite/background/bgwn6a08.png
	$(WINE) $(builddir)/img2sixel$(WINEEXT) $(top_srcdir)/images/pngsuite/background/bgyn6a16.png
	$(WINE) $(builddir)/img2sixel$(WINEEXT) -B\#fff $(top_srcdir)/images/pngsuite/background/bgai4a08.png
	$(WINE) $(builddir)/img2sixel$(WINEEXT) -B\#fff $(top_srcdir)/images/pngsuite/background/bgai4a16.png
	$(WINE) $(builddir)/img2sixel$(WINEEXT) -B\#fff $(top_srcdir)/images/pngsuite/background/bgan6a08.png
	$(WINE) $(builddir)/img2sixel$(WINEEXT) -B\#fff $(top_srcdir)/images/pngsuite/background/bgan6a16.png
	$(WINE) $(builddir)/img2sixel$(WINEEXT) -B\#fff $(top_srcdir)/images/pngsuite/background/bgbn4a08.png
	$(WINE) $(builddir)/img2sixel$(WINEEXT) -B\#fff $(top_srcdir)/images/pngsuite/background/bggn4a16.png
	$(WINE) $(builddir)/img2sixel$(WINEEXT) -B\#fff $(top_srcdir)/images/pngsuite/background/bgwn6a08.png
	$(WINE) $(builddir)/img2sixel$(WINEEXT) -B\#fff $(top_srcdir)/images/pngsuite/background/bgyn6a16.png
	$(WINE) $(builddir)/img2sixel$(WINEEXT) -w32 -B\#fff $(top_srcdir)/images/pngsuite/background/bgai4a08.png
	$(WINE) $(builddir)/img2sixel$(WINEEXT) -w32 -B\#fff $(top_srcdir)/images/pngsuite/background/bgai4a16.png
	$(WINE) $(builddir)/img2sixel$(WINEEXT) -w32 -B\#fff $(top_srcdir)/images/pngsuite/background/bgan6a08.png
	$(WINE) $(builddir)/img2sixel$(WINEEXT) -w32 -B\#fff $(top_srcdir)/images/pngsuite/background/bgan6a16.png
	$(WINE) $(builddir)/img2sixel$(WINEEXT) -w32 -B\#fff $(top_srcdir)/images/pngsuite/background/bgbn4a08.png
	$(WINE) $(builddir)/img2sixel$(WINEEXT) -w32 -B\#fff $(top_srcdir)/images/pngsuite/background/bggn4a16.png
	$(WINE) $(builddir)/img2sixel$(WINEEXT) -w32 -B\#fff $(top_srcdir)/images/pngsuite/background/bgwn6a08.png
	$(WINE) $(builddir)/img2sixel$(WINEEXT) -w32 -B\#fff $(top_srcdir)/images/pngsuite/background/bgyn6a16.png
endif
if HAVE_CURL
	@echo
	@echo '[test10] curl'
	test ! $$($(WINE) $(builddir)/img2sixel$(WINEEXT) file:///test)
	test ! $$($(WINE) $(builddir)/img2sixel$(WINEEXT) https:///test)
	$(WINE) $(builddir)/img2sixel$(WINEEXT) file:///$$(pwd)/$(top_srcdir)/images/snake.jpg
	if which openssl && which python; then \
	    openssl genrsa | openssl rsa > $(builddir)/tmp/server.key; \
	    openssl req -new -key $(builddir)/tmp/server.key -subj "/CN=localhost" | openssl x509 -req -signkey $(builddir)/tmp/server.key > $(builddir)/tmp/server.crt; \
	    echo "import BaseHTTPServer as bs, SimpleHTTPServer as ss, ssl" > $(builddir)/tmp/server.py; \
	    echo "httpd = bs.HTTPServer(('localhost', 4443), ss.SimpleHTTPRequestHandler)" >> $(builddir)/tmp/server.py; \
	    echo "httpd.socket = ssl.wrap_socket(httpd.socket, certfile='server.crt', keyfile='server.key', server_side=True)" >> $(builddir)/tmp/server.py; \
	    echo "httpd.handle_request()" >> $(builddir)/tmp/server.py; \
	    echo "httpd.handle_request()" >> $(builddir)/tmp/server.py; \
	    python $(builddir)/tmp/server.py & \
	    sleep 1; \
	    ! $(WINE) $(builddir)/img2sixel$(WINEEXT) 'https://localhost:4443/snake.sixel'; \
	    sleep 1; \
	    $(WINE) $(builddir)/img2sixel$(WINEEXT) -k 'https://localhost:4443/snake.sixel'; \
	fi
endif
if WANT_SIXEL2PNG
	@echo '[test11] sixel2png'
	test ! $$($(WINE) $(builddir)/sixel2png$(WINEEXT) -i $(builddir)/tmp/unknown.sixel)
	test ! $$($(WINE) $(builddir)/sixel2png$(WINEEXT) -% < $(builddir)/tmp/snake.sixel)
	test ! $$($(WINE) $(builddir)/sixel2png$(WINEEXT) invalid_filename < $(builddir)/tmp/snake.sixel)
	$(WINE) $(builddir)/sixel2png$(WINEEXT) -H
	$(WINE) $(builddir)/sixel2png$(WINEEXT) -V
	$(WINE) $(builddir)/sixel2png$(WINEEXT) < $(builddir)/tmp/snake.sixel > $(builddir)/tmp/snake1.png
	$(WINE) $(builddir)/sixel2png$(WINEEXT) < $(builddir)/tmp/snake2.sixel > $(builddir)/tmp/snake2.png
	$(WINE) $(builddir)/sixel2png$(WINEEXT) - - < $(builddir)/tmp/snake3.sixel > $(builddir)/tmp/snake3.png
	$(WINE) $(builddir)/sixel2png$(WINEEXT) -i $(builddir)/tmp/snake.sixel -o $(builddir)/tmp/snake4.png
endif
	@echo '[test13] regression test'
	$(WINE) $(builddir)/img2sixel$(WINEEXT) -B "#000" -B ""; test $$? = 255 -o $$? = 127
	$(WINE) $(builddir)/img2sixel$(WINEEXT) $(top_srcdir)/tests/issue/167/poc -h128 ; test $$? = 255 -o $$? = 127
	$(WINE) $(builddir)/img2sixel$(WINEEXT) $(top_srcdir)/tests/issue/166/poc -w128 ; test $$? = 255 -o $$? = 127
	$(WINE) $(builddir)/img2sixel$(WINEEXT) --7bit-mode -8 --invert --palette-type=auto --verbose $(top_srcdir)/tests/issue/200/POC_img2sixel_heap_buffer_overflow -o/dev/null

	@echo '[test14] documentation'
	$(WINE) $(builddir)/img2sixel$(WINEEXT) -H| grep ^\ \*\-|sed 's/^\ *//'| cut -f1 -d' '| cut -f1 -d, > $(builddir)/tmp/options1.txt
	grep ^\.B $(srcdir)/img2sixel.1  | cut -f2 -d " " | grep "^\\\\" | tr -d '\\' | tr -d , > $(builddir)/tmp/options2.txt
	grep ' --' $(srcdir)/shell-completion/bash/img2sixel| grep -v "' "| sed 's/.* \(-.\) .*/\1/' > $(builddir)/tmp/options3.txt
	grep '{-' $(srcdir)/shell-completion/zsh/_img2sixel|cut -f1 -d,|cut -f2 -d'{' > $(builddir)/tmp/options4.txt
	if which diff; then diff $(builddir)/tmp/options1.txt $(builddir)/tmp/options2.txt; fi
	if which diff; then diff $(builddir)/tmp/options2.txt $(builddir)/tmp/options3.txt; fi
	if which diff; then diff $(builddir)/tmp/options3.txt $(builddir)/tmp/options4.txt; fi
endif

	@echo '[succeeded]'

winetest: all
	WINE=wine $(MAKE) test
