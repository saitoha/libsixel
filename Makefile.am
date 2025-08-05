#
# @configure_input@
#
SUBDIRS = include src converters tools python
DIST_SUBDIRS = $(SUBDIRS)

EXTRA_DIST = LICENSE LICENSE.images LICENSE.mesa LICENSE.pngsuite \
	LICENSE.pnmcolormap LICENSE.sdump LICENSE.sixel LICENSE.stb

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

dist-windows: dist-zip
	mkdir -p @PACKAGE_NAME@-@PACKAGE_VERSION@/bin-windows
	cp converters/*.exe @PACKAGE_NAME@-@PACKAGE_VERSION@/bin-windows/
	zip -u @PACKAGE_NAME@-@PACKAGE_VERSION@.zip @PACKAGE_NAME@-@PACKAGE_VERSION@/bin-windows/*.exe
	mv -v @PACKAGE_NAME@-@PACKAGE_VERSION@.zip \
	    lib@PACKAGE_NAME@-@PACKAGE_VERSION@-$$(./config.status --config|tr \  \\n|grep ^--host|cut -f2 -d=).zip

dist-windows-i686-vexe:
	tools/vexe --host=i686-w64-mingw32 debian dist-windows
	docker ps -a --filter ancestor=libsixel-debian --format '{{.ID}}' | xargs -I{} docker cp {}:/tmp/libsixel/libsixel-1.8.6-i686-w64-mingw32.zip .

dist-windows-x86_64-vexe:
	tools/vexe --host=x86_64-w64-mingw32 debian dist-windows
	docker ps -a --filter ancestor=libsixel-debian --format '{{.ID}}' | xargs -I{} docker cp {}:/tmp/libsixel/libsixel-1.8.6-x86_64-w64-mingw32.zip .

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
