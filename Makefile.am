SUBDIRS = include src converters
DIST_SUBDIRS = $(SUBDIRS)

ACLOCAL_AMFLAGS = -I m4 ${ACLOCAL_FLAGS}

# pkgconfig
pkgconfigdir = @pkgconfigdir@
pkgconfig_DATA = libsixel.pc

CLEANFILES = libsixel.pc wic_install.reg wic_uninstall.reg

test: all
	$(MAKE) test -C converters

winetest: all
	$(MAKE) winetest -C converters

coveralls: test
	ln -s -f converters/stb_image.h .
	ln -s -f converters/stb_image_write.h .
	coveralls -e sixel_orig -e include -e m4
	rm -f stb_image.h
	rm -f stb_image_write.h
