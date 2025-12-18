#
# @configure_input@
#
SUBDIRS = include src converters assessment tools tests python
if WANT_AMALGAMATION
SUBDIRS += amalgamation
endif
if ENABLE_GDK_PIXBUF_LOADER
SUBDIRS += gdk-pixbuf-loader
endif
if HAVE_RUBY
SUBDIRS += ruby
endif
if HAVE_APPLE_QUICKLOOK_EXTENSION
SUBDIRS += quicklook-extension
endif
if HAVE_WICCODEC
SUBDIRS += wic
endif

DIST_SUBDIRS = \
        include \
        src \
        converters \
        assessment \
        tools \
        tests \
        python \
        ruby \
        wic \
        gdk-pixbuf-loader \
        quicklook-extension \
        amalgamation

# Keep depcomp in the aux directory so "make dist" can stage it correctly.
EXTRA_DIST = LICENSE build-aux/depcomp

EXTRA_DIST += examples/windows/wsh_decoder_sample.vbs

EXTRA_DIST += tests/issue/167/poc \
	tests/issue/166/poc \
	tests/issue/200/POC_img2sixel_heap_buffer_overflow \
	images/map8.png \
	images/autumn.png \
	images/egret.jpg \
	images/map16-palette.png \
	images/map16.png \
	images/map64.six \
	images/map8-palette.png \
	images/map8.png \
	images/map8.six \
	images/seq2gif.gif \
	images/snake-ascii.pbm \
	images/snake-ascii.pgm \
	images/snake-ascii.ppm \
	images/snake-grayscale.jpg \
	images/snake-grayscale.png \
	images/snake-palette.png \
	images/snake-progressive.jpg \
	images/snake.bmp \
	images/snake.gif \
	images/snake.jpg \
	images/snake.pbm \
	images/snake.pgm \
	images/snake.png \
	images/snake.ppm \
	images/snake.six \
	images/snake.tga \
	images/snake.tiff \
	images/pngsuite/background/bgwn6a08.png \
	images/pngsuite/background/bggn4a16.png \
	images/pngsuite/background/bgbn4a08.png \
	images/pngsuite/background/bgyn6a16.png \
	images/pngsuite/background/bgai4a08.png \
	images/pngsuite/background/bgan6a16.png \
	images/pngsuite/background/bgan6a08.png \
	images/pngsuite/background/bgai4a16.png \
	images/pngsuite/gamma/g03n3p04.png \
	images/pngsuite/gamma/g04n0g16.png \
	images/pngsuite/gamma/g05n0g16.png \
	images/pngsuite/gamma/g05n3p04.png \
	images/pngsuite/gamma/g04n3p04.png \
	images/pngsuite/gamma/g03n0g16.png \
	images/pngsuite/gamma/g07n2c08.png \
	images/pngsuite/gamma/g25n2c08.png \
	images/pngsuite/gamma/g10n2c08.png \
	images/pngsuite/gamma/g07n0g16.png \
	images/pngsuite/gamma/g03n2c08.png \
	images/pngsuite/gamma/g10n0g16.png \
	images/pngsuite/gamma/g25n0g16.png \
	images/pngsuite/gamma/g25n3p04.png \
	images/pngsuite/gamma/g10n3p04.png \
	images/pngsuite/gamma/g05n2c08.png \
	images/pngsuite/gamma/g04n2c08.png \
	images/pngsuite/gamma/g07n3p04.png \
	images/pngsuite/order/oi1n2c16.png \
	images/pngsuite/order/oi1n0g16.png \
	images/pngsuite/order/oi9n2c16.png \
	images/pngsuite/order/oi9n0g16.png \
	images/pngsuite/order/oi2n2c16.png \
	images/pngsuite/order/oi2n0g16.png \
	images/pngsuite/order/oi4n2c16.png \
	images/pngsuite/order/oi4n0g16.png \
	images/pngsuite/transparency/tbgn3p08.png \
	images/pngsuite/transparency/tbyn3p08.png \
	images/pngsuite/transparency/tbbn2c16.png \
	images/pngsuite/transparency/tm3n3p02.png \
	images/pngsuite/transparency/tbrn2c08.png \
	images/pngsuite/transparency/tbbn0g04.png \
	images/pngsuite/transparency/tbwn0g16.png \
	images/pngsuite/transparency/tp0n2c08.png \
	images/pngsuite/transparency/tp0n0g08.png \
	images/pngsuite/transparency/tbgn2c16.png \
	images/pngsuite/transparency/tbbn3p08.png \
	images/pngsuite/transparency/tp1n3p08.png \
	images/pngsuite/transparency/tp0n3p08.png \
	images/pngsuite/transparency/tbwn3p08.png \
	images/pngsuite/chunk/cs5n2c08.png \
	images/pngsuite/chunk/cm0n0g04.png \
	images/pngsuite/chunk/ccwn3p08.png \
	images/pngsuite/chunk/cdhn2c08.png \
	images/pngsuite/chunk/cs8n3p08.png \
	images/pngsuite/chunk/cthn0g04.png \
	images/pngsuite/chunk/cs3n3p08.png \
	images/pngsuite/chunk/ct0n0g04.png \
	images/pngsuite/chunk/ct1n0g04.png \
	images/pngsuite/chunk/cten0g04.png \
	images/pngsuite/chunk/cs5n3p08.png \
	images/pngsuite/chunk/cs8n2c08.png \
	images/pngsuite/chunk/ctzn0g04.png \
	images/pngsuite/chunk/cm7n0g04.png \
	images/pngsuite/chunk/ccwn2c08.png \
	images/pngsuite/chunk/ctjn0g04.png \
	images/pngsuite/chunk/cdun2c08.png \
	images/pngsuite/chunk/ch1n3p04.png \
	images/pngsuite/chunk/cs3n2c16.png \
	images/pngsuite/chunk/cm9n0g04.png \
	images/pngsuite/chunk/cdsn2c08.png \
	images/pngsuite/chunk/ch2n3p08.png \
	images/pngsuite/chunk/cdfn2c08.png \
	images/pngsuite/chunk/ctfn0g04.png \
	images/pngsuite/chunk/ctgn0g04.png \
	images/pngsuite/interlacing/basi0g08.png \
	images/pngsuite/interlacing/basi2c08.png \
	images/pngsuite/interlacing/basi3p08.png \
	images/pngsuite/interlacing/basi4a08.png \
	images/pngsuite/interlacing/basi6a08.png \
	images/pngsuite/interlacing/basi3p01.png \
	images/pngsuite/interlacing/basi4a16.png \
	images/pngsuite/interlacing/basi3p02.png \
	images/pngsuite/interlacing/basi3p04.png \
	images/pngsuite/interlacing/basi6a16.png \
	images/pngsuite/interlacing/basi0g02.png \
	images/pngsuite/interlacing/basi0g16.png \
	images/pngsuite/interlacing/basi2c16.png \
	images/pngsuite/interlacing/basi0g01.png \
	images/pngsuite/interlacing/basi0g04.png \
	images/pngsuite/basic/basn3p02.png \
	images/pngsuite/basic/basn4a16.png \
	images/pngsuite/basic/basn3p01.png \
	images/pngsuite/basic/basn6a16.png \
	images/pngsuite/basic/basn3p04.png \
	images/pngsuite/basic/basn0g01.png \
	images/pngsuite/basic/basn2c16.png \
	images/pngsuite/basic/basn0g02.png \
	images/pngsuite/basic/basn0g16.png \
	images/pngsuite/basic/basn0g04.png \
	images/pngsuite/basic/basn2c08.png \
	images/pngsuite/basic/basn0g08.png \
	images/pngsuite/basic/basn4a08.png \
	images/pngsuite/basic/basn3p08.png \
	images/pngsuite/basic/basn6a08.png \
	images/pngsuite/zlib/z03n2c08.png \
	images/pngsuite/zlib/z09n2c08.png \
	images/pngsuite/zlib/z06n2c08.png \
	images/pngsuite/zlib/z00n2c08.png \
	images/pngsuite/odd/s39i3p04.png \
	images/pngsuite/odd/s38i3p04.png \
	images/pngsuite/odd/s05n3p02.png \
	images/pngsuite/odd/s04n3p01.png \
	images/pngsuite/odd/s33i3p04.png \
	images/pngsuite/odd/s32i3p04.png \
	images/pngsuite/odd/s06i3p02.png \
	images/pngsuite/odd/s07i3p02.png \
	images/pngsuite/odd/s40n3p04.png \
	images/pngsuite/odd/s37n3p04.png \
	images/pngsuite/odd/s36n3p04.png \
	images/pngsuite/odd/s01i3p01.png \
	images/pngsuite/odd/s08n3p02.png \
	images/pngsuite/odd/s09n3p02.png \
	images/pngsuite/odd/s34i3p04.png \
	images/pngsuite/odd/s35i3p04.png \
	images/pngsuite/odd/s03n3p01.png \
	images/pngsuite/odd/s02n3p01.png \
	images/pngsuite/odd/s04i3p01.png \
	images/pngsuite/odd/s33n3p04.png \
	images/pngsuite/odd/s32n3p04.png \
	images/pngsuite/odd/s05i3p02.png \
	images/pngsuite/odd/s39n3p04.png \
	images/pngsuite/odd/s38n3p04.png \
	images/pngsuite/odd/s06n3p02.png \
	images/pngsuite/odd/s07n3p02.png \
	images/pngsuite/odd/s37i3p04.png \
	images/pngsuite/odd/s36i3p04.png \
	images/pngsuite/odd/s01n3p01.png \
	images/pngsuite/odd/s40i3p04.png \
	images/pngsuite/odd/s34n3p04.png \
	images/pngsuite/odd/s35n3p04.png \
	images/pngsuite/odd/s03i3p01.png \
	images/pngsuite/odd/s02i3p01.png \
	images/pngsuite/odd/s08i3p02.png \
	images/pngsuite/odd/s09i3p02.png \
	images/pngsuite/corrupted/xc1n0g08.png \
	images/pngsuite/corrupted/xs1n0g01.png \
	images/pngsuite/corrupted/xhdn0g08.png \
	images/pngsuite/corrupted/xs7n0g01.png \
	images/pngsuite/corrupted/xdtn0g01.png \
	images/pngsuite/corrupted/xcsn0g01.png \
	images/pngsuite/corrupted/xcrn0g04.png \
	images/pngsuite/corrupted/xd0n2c08.png \
	images/pngsuite/corrupted/xs2n0g01.png \
	images/pngsuite/corrupted/xc9n2c08.png \
	images/pngsuite/corrupted/xd9n2c08.png \
	images/pngsuite/corrupted/xlfn0g04.png \
	images/pngsuite/corrupted/xd3n2c08.png \
	images/pngsuite/corrupted/xs4n0g01.png \
	images/pngsuite/filter/f03n0g08.png \
	images/pngsuite/filter/f02n0g08.png \
	images/pngsuite/filter/f03n2c08.png \
	images/pngsuite/filter/f02n2c08.png \
	images/pngsuite/filter/f04n0g08.png \
	images/pngsuite/filter/f04n2c08.png \
	images/pngsuite/filter/f99n0g04.png \
	images/pngsuite/filter/f00n0g08.png \
	images/pngsuite/filter/f01n0g08.png \
	images/pngsuite/filter/f00n2c08.png \
	images/pngsuite/filter/f01n2c08.png \
	images/pngsuite/palette/ps1n2c16.png \
	images/pngsuite/palette/pp0n6a08.png \
	images/pngsuite/palette/ps2n0g08.png \
	images/pngsuite/palette/ps2n2c16.png \
	images/pngsuite/palette/ps1n0g08.png \
	images/pngsuite/palette/pp0n2c16.png


.PHONY: install-quicklook-extension uninstall-quicklook-extension

install-quicklook-extension:
	@HAVE_APPLE_QUICKLOOK_EXTENSION_TRUE@$(MAKE) -C quicklook-extension install-quicklook-extension
	@HAVE_APPLE_QUICKLOOK_EXTENSION_FALSE@{ echo "Quick Look extension is not enabled in this build" >&2; exit 1; }

uninstall-quicklook-extension:
	@HAVE_APPLE_QUICKLOOK_EXTENSION_TRUE@$(MAKE) -C quicklook-extension uninstall-quicklook-extension
	@HAVE_APPLE_QUICKLOOK_EXTENSION_FALSE@{ echo "Quick Look extension is not enabled in this build" >&2; exit 1; }

ACLOCAL_AMFLAGS = -I m4 ${ACLOCAL_FLAGS}

srcdir = @srcdir@
VPATH = @srcdir@

# pkgconfig
pkgconfigdir = @pkgconfigdir@
pkgconfig_DATA = $(builddir)/libsixel.pc

CLEANFILES = $(builddir)/valgrind.log $(builddir)/test.log $(builddir)/unittest.log \
             $(builddir)/package.json

all-am:
	cp $(builddir)/package.json.in $(builddir)/package.json

check-am:
	@:

install-data-local:
	@:

uninstall-local:
	@:

register:
	-regsvr32 -s "$(shell cygpath -w $(builddir)/wic/.libs/libwicsixel.dll)"
	@echo Registered: .libs/libwicsixel.dll

unregister:
	-regsvr32 -u -s "$(shell cygpath -w $(builddir)/wic/.libs/libwicsixel.dll)"
	@echo Unregistered: .libs/libwicsixel.dll

dist-windows: dist-zip
	mkdir -p @PACKAGE_NAME@-@PACKAGE_VERSION@/bin-windows
	cp converters/*.exe @PACKAGE_NAME@-@PACKAGE_VERSION@/bin-windows/
	zip -u @PACKAGE_NAME@-@PACKAGE_VERSION@.zip @PACKAGE_NAME@-@PACKAGE_VERSION@/bin-windows/*.exe
	mv -v @PACKAGE_NAME@-@PACKAGE_VERSION@.zip \
	    @PACKAGE_NAME@-@PACKAGE_VERSION@-$$(./config.status --config|tr \  \\n|grep ^--host|cut -f2 -d=).zip

dist-windows-i686-vexe:
	tools/vexe --host=i686-w64-mingw32 debian dist-windows
	docker ps -a --filter ancestor=libsixel-debian --format '{{.ID}}' | xargs -I{} docker cp {}:/tmp/libsixel/@PACKAGE_NAME@-@PACKAGE_VERSION@-i686-w64-mingw32.zip .

dist-windows-x86_64-vexe:
	tools/vexe --host=x86_64-w64-mingw32 debian dist-windows
	docker ps -a --filter ancestor=libsixel-debian --format '{{.ID}}' | xargs -I{} docker cp {}:/tmp/libsixel/@PACKAGE_NAME@-@PACKAGE_VERSION@-x86_64-w64-mingw32.zip .

unittest: all
	cd $(srcdir)/src && $(MAKE) unittest 2>&1 |tee $(builddir)/unittest.log

test: all
	cd src && $(MAKE) unittest 2>&1 |tee $(builddir)/test.log
	cd converters && $(MAKE) test 2>&1 |tee -a $(builddir)/test.log

winetest: all
	cd converters && $(MAKE) winetest

valgrind: all
	valgrind --leak-check=full --show-reachable=no --error-limit=no img2sixel images/snake.jpg -dfs 2>&1 \
		| grep ^== \
		| tee $(builddir)/valgrind.log
	grep "All heap blocks were freed" $(builddir)/valgrind.log || \
		grep "definitely lost: 0 bytes in 0 blocks" valgrind.log
	grep "All heap blocks were freed" $(builddir)/valgrind.log || \
		grep "indirectly lost: 0 bytes in 0 blocks" valgrind.log

coveralls:
	coveralls -e config.h -e stb_image.h -e stb_image_write.h -e examples -e include -e perl -e php -e ruby -e wic

coveralls-dryrun: test
	coveralls -e config.h -e stb_image.h -e stb_image_write.h -e examples -e include -e perl -e php -e ruby -e wic --dryrun

issue-131:
	converters/img2sixel --high-color tests/issue/131/2020-01-30-img2sixel.gif

issue-136:
	converters/img2sixel -8 tests/issue/136/poc

issue-150:
	converters/img2sixel -i tests/issue/150/poc -o tmp.sixel

issue-156:
	converters/img2sixel -o ./a.sixel -7 -p 1 -C 5 -d stucki -E size -i tests/issue/156/poc -o tmp.sixel

issue-156-vexe:
	AFL_USE_ASAN=1 CC=clang CFLAGS="-fsanitize=address -O0 -g" tools/vexe --disable-shared ubuntu issue-156

issue-163:
	converters/img2sixel tests/issue/163/poc0

issue-163-vexe:
	AFL_USE_ASAN=1 CC=clang CFLAGS="-fsanitize=address -O0 -g" tools/vexe --disable-shared ubuntu issue-163

issue-165:
	converters/img2sixel tests/issue/165/poc

issue-165-vexe:
	tools/vexe --without-png ubuntu issue-165

issue-166:
	converters/img2sixel ../tests/issue/166/poc -w128 ; test $$? = 255 -o $$? = 127

issue-166-vexe:
	tools/vexe --platform=amd64 ubuntu issue-166

issue-167:
	converters/img2sixel ../tests/issue/167/poc -h128 ; test $$? = 255 -o $$? = 127

issue-167-vexe:
	tools/vexe --platform=amd64 ubuntu issue-167

issue-200:
	converters/img2sixel --7bit-mode -8 --invert --palette-type=auto --verbose tests/issue/200/POC_img2sixel_heap_buffer_overflow -o /dev/null

issue-207: all
	converters/img2sixel -h 50% -r lanczos3 -w 300px tests/issue/207/poc; test $$? = 255 -o $$? = 127

issue-207-vexe:
	CC=clang CFLAGS="-fsanitize=address -O0 -g -fno-omit-frame-pointer" tools/vexe ubuntu issue-207

libsixel-libsixel-issue-73:
	converters/img2sixel tests/issue/libsixel-libsixel/73/stbi_1561_poc.bin

libsixel-libsixel-issue-51:
	converters/img2sixel tests/issue/libsixel-libsixel/51/stbi_1561_poc\ 2.bin
