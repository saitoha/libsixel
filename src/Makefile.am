
if COND_GCOV
MAYBE_COVERAGE = --coverage
endif

CLEANFILES = *.gcno *.gcda *.gcov

lib_LTLIBRARIES = libsixel.la
libsixel_la_SOURCES = output.c fromsixel.c tosixel.c quant.c dither.c
libsixel_la_CPPFLAGS = -I$(top_builddir)/include/
libsixel_la_CFLAGS = $(CFLAGS) $(AM_CFLAGS) $(MAYBE_COVERAGE)
libsixel_la_LDFLAGS = -version-info $(LS_LTVERSION)
dist_man_MANS = sixel.5
