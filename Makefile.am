SUBDIRS = include src
DIST_SUBDIRS = $(SUBDIRS)

ACLOCAL_AMFLAGS = -I m4 ${ACLOCAL_FLAGS}

test: all
	$(MAKE) test -C src
