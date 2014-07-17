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
	valgrind --leak-check=full --show-reachable=no --error-limit=no img2sixel images/snake.jpg -dfs 2>&1 \
		| grep ^== \
		| tee valgrind.log
	grep "All heap blocks were freed" valgrind.log || \
		grep "definitely lost: 0 bytes in 0 blocks" valgrind.log
	grep "All heap blocks were freed" valgrind.log || \
		grep "indirectly lost: 0 bytes in 0 blocks" valgrind.log

coveralls: test
	ln -sf converters/stb_image.h .
	ln -sf converters/stb_image.h.gcov .
	ln -sf converters/stb_image_write.h .
	ln -sf converters/stb_image_write.h.gcov .
	coveralls -e sixel_orig -e include -e m4
	rm -f stb_image.h
	rm -f stb_image.h.gcov
	rm -f stb_image_write.h
	rm -f stb_image_write.h.gcov

coveralls-dryrun: test
	ln -sf converters/stb_image.h .
	ln -sf converters/stb_image.h.gcov .
	ln -sf converters/stb_image_write.h .
	ln -sf converters/stb_image_write.h.gcov .
	coveralls -e sixel_orig -e include -e m4 --dryrun
	rm -f stb_image.h
	rm -f stb_image.h.gcov
	rm -f stb_image_write.h
	rm -f stb_image_write.h.gcov
