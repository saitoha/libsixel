
CLEANFILES =

clean-local:

bin_SCRIPTS = libsixel-config

bin_PROGRAMS = sixelcheck
sixelcheck_SOURCES = sixelcheck.c
sixelcheck_CPPFLAGS = -I$(top_builddir)/include/ $(AM_CPPFLAGS)
sixelcheck_CFLAGS = $(CFLAGS) $(AM_CFLAGS) $(MAYBE_COVERAGE)
sixelcheck_LDADD =

EXTRA_DIST = libsixel-config.in
