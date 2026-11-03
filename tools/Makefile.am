
CLEANFILES = compare/cmp compare/cmp.exe compare.obj \
    compare/cmp.pdb compare/cmp.ilk compare/cmp.exe.manifest

clean-local:

bin_SCRIPTS = libsixel-config

EXTRA_DIST = libsixel-config.in compare.c

# The comparator executes on the build machine even when the library is cross-built.
if BUILD_TEST_COMPARE
noinst_DATA = compare/cmp$(SIXEL_COMPARE_EXEEXT)
endif
compare/cmp$(SIXEL_COMPARE_EXEEXT): $(srcdir)/compare.c Makefile
	$(AM_V_at)$(MKDIR_P) compare
	$(AM_V_CC)$(CC_FOR_BUILD) $(CFLAGS_FOR_BUILD) -o $@ $(srcdir)/compare.c
