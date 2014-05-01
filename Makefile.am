SUBDIRS = include src converters
DIST_SUBDIRS = $(SUBDIRS)

ACLOCAL_AMFLAGS = -I m4 ${ACLOCAL_FLAGS}

test: all
	$(MAKE) test -C converters

winetest: all
	$(MAKE) winetest -C converters
