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

CLEANFILES = *.gcno *.gcda *.gcov *.png *.sixel *.txt *.pipe server.py server.key server.crt testfile

if WANT_IMG2SIXEL
bin_PROGRAMS += img2sixel
img2sixel_SOURCES = img2sixel.c malloc_stub.c malloc_stub.h
img2sixel_CPPFLAGS = -I$(top_builddir)/include/ $(AM_CPPFLAGS)
img2sixel_CFLAGS = $(CFLAGS) $(AM_CFLAGS) $(MAYBE_COVERAGE)
img2sixel_LDADD = $(top_builddir)/src/libsixel.la
dist_man_MANS += img2sixel.1
dist_bashcompletion_DATA += shell-completion/bash/img2sixel
dist_zshcompletion_DATA += shell-completion/zsh/_img2sixel
endif

if WANT_SIXEL2PNG
bin_PROGRAMS += sixel2png
sixel2png_SOURCES = sixel2png.c malloc_stub.c malloc_stub.h
sixel2png_CPPFLAGS = -I$(top_builddir)/include/ $(AM_CPPFLAGS)
sixel2png_CFLAGS = $(CFLAGS) $(AM_CFLAGS) $(MAYBE_COVERAGE)
sixel2png_LDADD = $(top_builddir)/src/libsixel.la
dist_man_MANS += sixel2png.1
endif

check: test_impl

test: all test_impl

test_impl:
if WANT_IMG2SIXEL
	touch testfile
	chmod -r testfile
	test ! $$($(WINE) ./img2sixel testfile)
	test ! $$($(WINE) ./img2sixel invalid_filename)
	test ! $$($(WINE) ./img2sixel .)
	test ! $$($(WINE) ./img2sixel -d invalid_option)
	test ! $$($(WINE) ./img2sixel -r invalid_option)
	test ! $$($(WINE) ./img2sixel -s invalid_option)
	test ! $$($(WINE) ./img2sixel -t invalid_option)
	test ! $$($(WINE) ./img2sixel -w invalid_option)
	test ! $$($(WINE) ./img2sixel -h invalid_option)
	test ! $$($(WINE) ./img2sixel -f invalid_option)
	test ! $$($(WINE) ./img2sixel -q invalid_option)
	test ! $$($(WINE) ./img2sixel -l invalid_option)
	test ! $$($(WINE) ./img2sixel -b invalid_option)
	test ! $$($(WINE) ./img2sixel -E invalid_option)
	test ! $$($(WINE) ./img2sixel -B invalid_option)
	test ! $$($(WINE) ./img2sixel -B \#ffff ../images/map8.png)
	test ! $$($(WINE) ./img2sixel -B \#0000000000000 ../images/map8.png)
	test ! $$($(WINE) ./img2sixel -B \#00G)
	test ! $$($(WINE) ./img2sixel -B test)
	test ! $$($(WINE) ./img2sixel -B rgb:11/11)
	test ! $$($(WINE) ./img2sixel -%)
	test ! $$($(WINE) ./img2sixel -m invalid_filename ../images/snake.jpg)
	test ! $$($(WINE) ./img2sixel -m ../images/map8.png -p 8 ../images/snake.jpg)
	test ! $$($(WINE) ./img2sixel -m ../images/map8.png -e ../images/snake.jpg)
	test ! $$($(WINE) ./img2sixel -p16 -e ../images/snake.jpg)
	test ! $$($(WINE) ./img2sixel -I -C0 ../images/snake.png)
	test ! $$($(WINE) ./img2sixel -I -e ../images/snake.png)
	test ! $$($(WINE) ./img2sixel -I -p8 ../images/snake.png)
	test ! $$($(WINE) ./img2sixel -I -m ../images/map8.png ../images/snake.png)
	test ! $$($(WINE) ./img2sixel -I -bxterm16 ../images/snake.png)
	test ! $$($(WINE) ./img2sixel -e -bxterm256 ../images/snake.png)
	test ! $$($(WINE) ./img2sixel -p64 -bxterm256 ../images/snake.png)
	test ! $$($(WINE) ./img2sixel -m ../images/map16.png -bxterm256 ../images/snake.png)
	test ! $$($(WINE) ./img2sixel -8 -P ../images/snake.png)
	test ! $$(echo -n a | $(WINE) ./img2sixel)
	$(WINE) ./img2sixel -H
	$(WINE) ./img2sixel -V
	$(WINE) ./img2sixel ../images/snake.jpg -datkinson -flum -saverage | ./img2sixel | tee snake.sixel
	$(WINE) ./img2sixel -w50% -h150% -dfs -Bblue -thls -shistogram < ../images/snake.jpg | tee snake2.sixel
	$(WINE) ./img2sixel -w2100 -h2100 -djajuni -bxterm256 -o snake3.sixel < ../images/snake.jpg
	$(WINE) ./img2sixel -w105% -h100 -B\#000000000 -rnearest < ../images/snake.gif
	$(WINE) ./img2sixel -7 -sauto -w100 -rgaussian -qauto -dburkes -tauto ../images/snake.tga
	$(WINE) ./img2sixel -p300 -8 -scenter -Brgb:0/f/A -h100 -qfull -rhanning -dstucki -thls ../images/snake.tiff
	$(WINE) ./img2sixel -8 -qauto -thls -e ../images/snake.pgm
	$(WINE) ./img2sixel -8 -m ../images/map8-palette.png -Esize ../images/snake.ppm
	$(WINE) ./img2sixel -7 -m ../images/map16-palette.png -Efast ../images/snake.jpg
	$(WINE) ./img2sixel -7 -w300 ../images/snake-palette.png
	$(WINE) ./img2sixel -7 -w100 -h100 -bxterm16 -B\#aB3 -B\#aB3 ../images/snake.pbm
	$(WINE) ./img2sixel -I -dstucki -thls -B\#a0B030 ../images/snake.ppm
	$(WINE) ./img2sixel -bvt340color ../images/snake.ppm
	$(WINE) ./img2sixel -bvt340mono ../images/snake.tga
	$(WINE) ./img2sixel -bgray1 -w120 ../images/snake.tga
	$(WINE) ./img2sixel -bgray2 -w120 ../images/snake.tga
	$(WINE) ./img2sixel -bgray4 -w120 ../images/snake.tga
	$(WINE) ./img2sixel -bgray8 -w120 ../images/snake.tga
	$(WINE) ./img2sixel -I -8 -dburkes -B\#ffffffffffff ../images/snake-ascii.ppm
	$(WINE) ./img2sixel -I -C10 -djajuni ../images/snake.png
	$(WINE) ./img2sixel -I -Eauto ../images/snake-ascii.pgm
	$(WINE) ./img2sixel -I -datkinson ../images/snake-ascii.pbm
	$(WINE) ./img2sixel ../images/snake-grayscale.png
	$(WINE) ./img2sixel -m ../images/map8-palette.png ../images/snake-grayscale.png
	$(WINE) ./img2sixel -m ../images/snake-grayscale.png ../images/snake.png
	$(WINE) ./img2sixel -c200x200+100+100 ../images/snake-grayscale.png
	$(WINE) ./img2sixel -c200x200+100+100 -w400 ../images/snake-grayscale.png
	$(WINE) ./img2sixel -I ../images/snake-grayscale.png
	$(WINE) ./img2sixel -I ../images/snake-grayscale.jpg
	$(WINE) ./img2sixel -m ../images/map8.six -m ../images/map8.six ../images/snake.six
	$(WINE) ./img2sixel -w200 -p8 ../images/snake.six
	$(WINE) ./img2sixel -c200x200+2000+2000 ../images/snake.six
	$(WINE) ./img2sixel -bxterm16 ../images/snake.six
	$(WINE) ./img2sixel -e ../images/snake.six
	$(WINE) ./img2sixel -I ../images/snake.six
	$(WINE) ./img2sixel -I -c2000x100+40+20 -wauto -h200 -qhigh -dfs -rbilinear -trgb ../images/snake.ppm
	$(WINE) ./img2sixel -I -v -w200 -hauto -c100x1000+40+20 -qlow -dnone -rhamming -thls ../images/snake.bmp
	$(WINE) ./img2sixel -m ../images/map8.png -w200 -fauto -rwelsh ../images/egret.jpg
	$(WINE) ./img2sixel -m ../images/map16.png -w100 -hauto -rbicubic -dauto ../images/snake.ppm
	$(WINE) ./img2sixel -p 16 -C3 -h100 -fnorm -rlanczos2 ../images/snake.jpg
	$(WINE) ./img2sixel -v -p 8 -h200 -fnorm -rlanczos2 -dnone ../images/snake.jpg
	$(WINE) ./img2sixel -p 2 -h100 -wauto -rlanczos3 ../images/snake.jpg
	$(WINE) ./img2sixel -p 1 -h100 -n1 ../images/snake.jpg && printf '\033[*1z'
	$(WINE) ./img2sixel -e -h140 -rlanczos4 -P ../images/snake.jpg
	$(WINE) ./img2sixel -e -i -P ../images/snake.jpg > /dev/null
	$(WINE) ./img2sixel -w2047 -h2047 ../images/snake.png | $(WINE) ./img2sixel > /dev/null
	seq 0 10 | while read i; do \
	    seq 0 2 | while read j; do \
		    $(WINE) ./img2sixel ../images/map8.png | \
	            sed "s/Pq/P$${i};;$${j}q/" | \
	            $(WINE) ./img2sixel; \
	    done; \
	done
	$(WINE) ./img2sixel ../images/snake.png| sed 's/C/C:/g'| tr : '\t'| $(WINE) ./img2sixel
	$(WINE) ./img2sixel ../images/snake.png| sed 's/"1;1;600;450/"1;1;700;500/'| $(WINE) ./img2sixel
	$(WINE) ./img2sixel -ldisable -dnone -u -lauto ../images/seq2gif.gif
	$(WINE) ./img2sixel -ldisable -dnone -g ../images/seq2gif.gif
	$(WINE) ./img2sixel -ldisable -dnone -u -g ../images/seq2gif.gif
	$(WINE) ./img2sixel -S -datkinson ../images/seq2gif.gif
	$(WINE) ./img2sixel ../images/snake-progressive.jpg
if HAVE_PNG
	$(WINE) ./img2sixel ../images/pngsuite/basic/basn0g01.png
	$(WINE) ./img2sixel ../images/pngsuite/basic/basn0g02.png
	$(WINE) ./img2sixel ../images/pngsuite/basic/basn0g04.png
	$(WINE) ./img2sixel ../images/pngsuite/basic/basn0g08.png
	$(WINE) ./img2sixel ../images/pngsuite/basic/basn0g16.png
	$(WINE) ./img2sixel ../images/pngsuite/basic/basn3p01.png
	$(WINE) ./img2sixel ../images/pngsuite/basic/basn3p02.png
	$(WINE) ./img2sixel ../images/pngsuite/basic/basn3p04.png
	$(WINE) ./img2sixel ../images/pngsuite/basic/basn3p08.png
	$(WINE) ./img2sixel ../images/pngsuite/basic/basn4a08.png
	$(WINE) ./img2sixel ../images/pngsuite/basic/basn4a16.png
	$(WINE) ./img2sixel ../images/pngsuite/basic/basn6a08.png
	$(WINE) ./img2sixel ../images/pngsuite/basic/basn6a16.png
	$(WINE) ./img2sixel -w32 ../images/pngsuite/basic/basn0g01.png
	$(WINE) ./img2sixel -w32 ../images/pngsuite/basic/basn0g02.png
	$(WINE) ./img2sixel -w32 ../images/pngsuite/basic/basn0g04.png
	$(WINE) ./img2sixel -w32 ../images/pngsuite/basic/basn0g08.png
	$(WINE) ./img2sixel -w32 ../images/pngsuite/basic/basn0g16.png
	$(WINE) ./img2sixel -w32 ../images/pngsuite/basic/basn3p01.png
	$(WINE) ./img2sixel -w32 ../images/pngsuite/basic/basn3p02.png
	$(WINE) ./img2sixel -w32 ../images/pngsuite/basic/basn3p04.png
	$(WINE) ./img2sixel -w32 ../images/pngsuite/basic/basn3p08.png
	$(WINE) ./img2sixel -w32 ../images/pngsuite/basic/basn4a08.png
	$(WINE) ./img2sixel -w32 ../images/pngsuite/basic/basn4a16.png
	$(WINE) ./img2sixel -w32 ../images/pngsuite/basic/basn6a08.png
	$(WINE) ./img2sixel -w32 ../images/pngsuite/basic/basn6a16.png
	$(WINE) ./img2sixel -c16x16+8+8 ../images/pngsuite/basic/basn0g01.png
	$(WINE) ./img2sixel -c16x16+8+8 ../images/pngsuite/basic/basn0g02.png
	$(WINE) ./img2sixel -c16x16+8+8 ../images/pngsuite/basic/basn0g04.png
	$(WINE) ./img2sixel -c16x16+8+8 ../images/pngsuite/basic/basn0g08.png
	$(WINE) ./img2sixel -c16x16+8+8 ../images/pngsuite/basic/basn0g16.png
	$(WINE) ./img2sixel -c16x16+8+8 ../images/pngsuite/basic/basn3p01.png
	$(WINE) ./img2sixel -c16x16+8+8 ../images/pngsuite/basic/basn3p02.png
	$(WINE) ./img2sixel -c16x16+8+8 ../images/pngsuite/basic/basn3p04.png
	$(WINE) ./img2sixel -c16x16+8+8 ../images/pngsuite/basic/basn3p08.png
	$(WINE) ./img2sixel -c16x16+8+8 ../images/pngsuite/basic/basn4a08.png
	$(WINE) ./img2sixel -c16x16+8+8 ../images/pngsuite/basic/basn4a16.png
	$(WINE) ./img2sixel -c16x16+8+8 ../images/pngsuite/basic/basn6a08.png
	$(WINE) ./img2sixel -c16x16+8+8 ../images/pngsuite/basic/basn6a16.png
	$(WINE) ./img2sixel ../images/pngsuite/background/bgai4a08.png
	$(WINE) ./img2sixel ../images/pngsuite/background/bgai4a16.png
	$(WINE) ./img2sixel ../images/pngsuite/background/bgan6a08.png
	$(WINE) ./img2sixel ../images/pngsuite/background/bgan6a16.png
	$(WINE) ./img2sixel ../images/pngsuite/background/bgbn4a08.png
	$(WINE) ./img2sixel ../images/pngsuite/background/bggn4a16.png
	$(WINE) ./img2sixel ../images/pngsuite/background/bgwn6a08.png
	$(WINE) ./img2sixel ../images/pngsuite/background/bgyn6a16.png
	$(WINE) ./img2sixel -B\#fff ../images/pngsuite/background/bgai4a08.png
	$(WINE) ./img2sixel -B\#fff ../images/pngsuite/background/bgai4a16.png
	$(WINE) ./img2sixel -B\#fff ../images/pngsuite/background/bgan6a08.png
	$(WINE) ./img2sixel -B\#fff ../images/pngsuite/background/bgan6a16.png
	$(WINE) ./img2sixel -B\#fff ../images/pngsuite/background/bgbn4a08.png
	$(WINE) ./img2sixel -B\#fff ../images/pngsuite/background/bggn4a16.png
	$(WINE) ./img2sixel -B\#fff ../images/pngsuite/background/bgwn6a08.png
	$(WINE) ./img2sixel -B\#fff ../images/pngsuite/background/bgyn6a16.png
	$(WINE) ./img2sixel -w32 -B\#fff ../images/pngsuite/background/bgai4a08.png
	$(WINE) ./img2sixel -w32 -B\#fff ../images/pngsuite/background/bgai4a16.png
	$(WINE) ./img2sixel -w32 -B\#fff ../images/pngsuite/background/bgan6a08.png
	$(WINE) ./img2sixel -w32 -B\#fff ../images/pngsuite/background/bgan6a16.png
	$(WINE) ./img2sixel -w32 -B\#fff ../images/pngsuite/background/bgbn4a08.png
	$(WINE) ./img2sixel -w32 -B\#fff ../images/pngsuite/background/bggn4a16.png
	$(WINE) ./img2sixel -w32 -B\#fff ../images/pngsuite/background/bgwn6a08.png
	$(WINE) ./img2sixel -w32 -B\#fff ../images/pngsuite/background/bgyn6a16.png
endif
if HAVE_CURL
	test ! $$($(WINE) ./img2sixel file:///test)
	test ! $$($(WINE) ./img2sixel https:///test)
	$(WINE) ./img2sixel file:///$$(pwd)/../images/snake.jpg
	if which openssl && which python; then \
	    openssl genrsa | openssl rsa > server.key; \
	    openssl req -new -key server.key -subj "/CN=localhost" | openssl x509 -req -signkey server.key > server.crt; \
	    echo "import BaseHTTPServer as bs, SimpleHTTPServer as ss, ssl" > server.py; \
	    echo "httpd = bs.HTTPServer(('localhost', 4443), ss.SimpleHTTPRequestHandler)" >> server.py; \
	    echo "httpd.socket = ssl.wrap_socket(httpd.socket, certfile='server.crt', keyfile='server.key', server_side=True)" >> server.py; \
	    echo "httpd.handle_request()" >> server.py; \
	    echo "httpd.handle_request()" >> server.py; \
	    python server.py& \
	    sleep 1; \
	    ! ./img2sixel 'https://localhost:4443/snake.sixel'; \
	    sleep 1; \
	    ./img2sixel -k 'https://localhost:4443/snake.sixel'; \
	fi
endif
if WANT_SIXEL2PNG
	test ! $$($(WINE) ./sixel2png -i unknown.sixel)
	test ! $$($(WINE) ./sixel2png -% < snake.sixel)
	test ! $$($(WINE) ./sixel2png invalid_filename < snake.sixel)
	$(WINE) ./sixel2png -H
	$(WINE) ./sixel2png -V
	$(WINE) ./sixel2png < snake.sixel > snake1.png
	$(WINE) ./sixel2png < snake2.sixel > snake2.png
	$(WINE) ./sixel2png - - < snake3.sixel > snake3.png
	$(WINE) ./sixel2png -i snake.sixel -o snake4.png
endif
	$(WINE) ./img2sixel -H| grep ^\ \*\-|sed 's/^\ *//'| cut -f1 -d' '| cut -f1 -d, > options1.txt
	rm -f sixel.pipe
	if test "$(WINE)" = "" && which mkfifo && which pkill; then \
	    mkfifo sixel.pipe; \
	    ($(WINE) ./img2sixel -D < sixel.pipe&); \
	    cat ../images/snake.jpg > sixel.pipe; \
	    echo -n > sixel.pipe; \
	    sleep 1; \
	    rm -f sixel.pipe; \
	    pkill img2sixel; \
	fi || true
	grep ^\.B img2sixel.1  | cut -f2 -d " " | grep "^\\\\" | tr -d '\\' | tr -d , > options2.txt
	grep ' --' shell-completion/bash/img2sixel| grep -v "' "| sed 's/.* \(-.\) .*/\1/' > options3.txt
	grep '{-' shell-completion/zsh/_img2sixel|cut -f1 -d,|cut -f2 -d'{' > options4.txt
	diff options1.txt options2.txt
	diff options2.txt options3.txt
	diff options3.txt options4.txt
endif
	@echo succeeded

winetest: all
	WINE=wine $(MAKE) test
