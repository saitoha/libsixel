SUBDIRS = include src converters
DIST_SUBDIRS = $(SUBDIRS)

ACLOCAL_AMFLAGS = -I m4 ${ACLOCAL_FLAGS}

# pkgconfig
pkgconfigdir = @pkgconfigdir@
pkgconfig_DATA = libsixel.pc

CLEANFILES = libsixel.pc wic_install.reg wic_uninstall.reg valgrind.log

test: all
	$(MAKE) test -C converters

winetest: all
	$(MAKE) winetest -C converters

valgrind: all
	valgrind --leak-check=full --show-leachable=no --error-limit=no img2sixel images/snake.jpg -dfs 2>&1 \
		| grep ^== \
		> tee valgrind.log
	grep "definitely lost: 0 bytes in 0 blocks" valgrind.log
	grep "indirectly lost: 0 bytes in 0 blocks" valgrind.log

coveralls: test
	rm -rf gcovtmp
	mkdir gcovtmp
	cp src/*.{c,cc,gcno,gcda} converters/*.{c,h,gcno,gcda} .coveralls.yml gcovtmp/
	cd gcovtmp && coveralls -e sixel_orig -e include -e m4
	rm -rf gcovtmp

coveralls-dryrun: test
	rm -rf gcovtmp
	mkdir gcovtmp
	cp src/*.{c,cc,gcno,gcda} converters/*.{c,h,gcno,gcda} .coveralls.yml gcovtmp/
	cd gcovtmp && coveralls -e sixel_orig -e include -e m4 --dryrun
	rm -rf gcovtmp
