#
# @configure_input@
#
SUBDIRS = include src converters tools python
if HAVE_RUBY
SUBDIRS += ruby
endif
if HAVE_APPLE_QUICKLOOK_EXTENSION
SUBDIRS += quicklook-extension
endif
if HAVE_WICCODEC
SUBDIRS += wic
endif

DIST_SUBDIRS = include src converters tools python ruby wic quicklook-extension

EXTRA_DIST = LICENSE depcomp

EXTRA_DIST += tests/converters/common.bash \
	tests/converters/test_img2sixel_0001_invalid_option.bash \
	tests/converters/test_img2sixel_0002_stdin.bash \
	tests/converters/test_img2sixel_0003_information.bash \
	tests/converters/test_img2sixel_0004_conversion_options.bash \
	tests/converters/test_img2sixel_0005_dcs_arguments.bash \
	tests/converters/test_img2sixel_0006_dcs_variations.bash \
	tests/converters/test_img2sixel_0007_animation.bash \
	tests/converters/test_img2sixel_0008_progressive_jpeg.bash \
	tests/converters/test_img2sixel_0009_pngsuite.bash \
	tests/converters/test_img2sixel_0010_curl.bash \
	tests/converters/test_sixel2png_0011_basic.bash \
	tests/converters/test_img2sixel_0012_regressions.bash \
	tests/converters/test_img2sixel_0013_documentation.bash

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

CLEANFILES = $(builddir)/valgrind.log $(builddir)/test.log $(builddir)/unittest.log

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
