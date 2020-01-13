#
# @configure_input@
#
SUBDIRS = include src converters tools python
DIST_SUBDIRS = $(SUBDIRS)

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
