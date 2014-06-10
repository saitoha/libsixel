SUBDIRS = include src converters
DIST_SUBDIRS = $(SUBDIRS)

ACLOCAL_AMFLAGS = -I m4 ${ACLOCAL_FLAGS}

# pkgconfig
pkgconfigdir = @pkgconfigdir@
pkgconfig_DATA = libsixel.pc

CLEANFILES = libsixel.pc wic_install.reg wic_uninstall.reg valgrind.log

test: all
	$(MAKE) test -C converters > test.log

winetest: all
	$(MAKE) winetest -C converters

valgrind: all
	valgrind --leak-check=full --show-leachable=no --error-limit=no img2sixel images/snake.jpg -dfs 2>&1 \
		| grep ^== \
		| tee valgrind.log \
		| grep 'ERROR SUMMARY: 0 errors from 0 contexts'

coveralls: test
	rm -rf gcovtmp
	mkdir gcovtmp
	cp src/*.{c,cc,gcno,gcda} converters/*.{c,h,gcno,gcda} .coveralls.yml gcovtmp/
	coveralls -e sixel_orig -e include -e m4
	rm -rf gcovtmp

coveralls-dryrun: test
	rm -rf gcovtmp
	mkdir gcovtmp
	cp src/*.{c,cc,gcno,gcda} converters/*.{c,h,gcno,gcda} .coveralls.yml gcovtmp/
	coveralls -e sixel_orig -e include -e m4 --dryrun
	rm -rf gcovtmp
