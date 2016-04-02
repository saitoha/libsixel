SUBDIRS = include src converters tools python
DIST_SUBDIRS = $(SUBDIRS)

ACLOCAL_AMFLAGS = -I m4 ${ACLOCAL_FLAGS}

# pkgconfig
pkgconfigdir = @pkgconfigdir@
pkgconfig_DATA = libsixel.pc

CLEANFILES = valgrind.log test.log unittest.log test-output.png

all-am:
	cp package.json.in package.json

check-am:

unittest: all
	cd src && $(MAKE) unittest 2>&1 |tee unittest.log

test: all
	cd src && $(MAKE) unittest 2>&1 |tee ../test.log
	cd converters && $(MAKE) test 2>&1 |tee -a ../test.log

winetest: all
	cd converters && $(MAKE) winetest

valgrind: all
	valgrind --leak-check=full --show-reachable=no --error-limit=no img2sixel images/snake.jpg -dfs 2>&1 \
		| grep ^== \
		| tee valgrind.log
	grep "All heap blocks were freed" valgrind.log || \
		grep "definitely lost: 0 bytes in 0 blocks" valgrind.log
	grep "All heap blocks were freed" valgrind.log || \
		grep "indirectly lost: 0 bytes in 0 blocks" valgrind.log

coveralls:
	coveralls -e sixel_orig -e include -e m4 \
		-e stb_image.h -e stb_image_write.h

coveralls-dryrun: test
	coveralls -e sixel_orig -e include -e m4 \
		-e stb_image.h -e stb_image_write.h \
		--dryrun
