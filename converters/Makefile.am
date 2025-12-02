DIST_SUBDIRS = $(SUBDIRS)

srcdir = @srcdir@
top_builddir = @top_builddir@
builddir = @builddir@
VPATH = @srcdir@

BUILT_SOURCES = completion_embed.h

BASH = @BASH@

EXTRA_DIST = $(srcdir)/aborttrace.c $(srcdir)/aborttrace.h

bin_PROGRAMS =
dist_man_MANS =

bashcompletiondir = @bashcompletiondir@
dist_bashcompletion_DATA =

zshcompletiondir = @zshcompletiondir@
dist_zshcompletion_DATA =

if COND_GCOV
MAYBE_COVERAGE = --coverage
endif

CLEANFILES = *.gcno *.gcda *.gcov \
  $(builddir)/tmp/*.png \
  $(builddir)/tmp/*.sixel \
  $(builddir)/tmp/*.txt \
  $(builddir)/tmp/*.pipe \
  $(builddir)/tmp/server.py \
  $(builddir)/tmp/server.key \
  $(builddir)/tmp/server.crt \
  $(builddir)/tmp/testfile \
  completion_embed.h

# +--------------------------------------------------------------+
# | Embedded completion header generation pipeline (ASCII view). |
# +--------------------------------------------------------------+
# bash/img2sixel ----\
#                      >--[gen_completion_embed.awk]--> completion_embed.h
# zsh/_img2sixel  ----/
completion_embed.h: \
	$(srcdir)/shell-completion/bash/img2sixel \
	$(srcdir)/shell-completion/zsh/_img2sixel \
	$(top_srcdir)/tools/gen_completion_embed.awk
	$(AM_V_GEN)$(AWK) -f $(top_srcdir)/tools/gen_completion_embed.awk \
	       $(srcdir)/shell-completion/bash/img2sixel \
	       $(srcdir)/shell-completion/zsh/_img2sixel > $@

if WANT_IMG2SIXEL
bin_PROGRAMS += img2sixel
img2sixel_SOURCES = $(srcdir)/img2sixel.c \
                    $(srcdir)/completion_utils.c \
                    $(srcdir)/completion_utils.h \
                    $(srcdir)/cli.c \
                    $(srcdir)/cli.h \
                    $(srcdir)/malloc_stub.c \
                    $(srcdir)/malloc_stub.h \
                    $(srcdir)/getopt_stub.h \
                    $(srcdir)/aborttrace.c \
                    $(srcdir)/aborttrace.h
# Track the generated header so dependency ordering stays stable.
nodist_img2sixel_SOURCES = completion_embed.h
# Ensure older toolchains notice the generated completion header.
img2sixel_CPPFLAGS = -DHAVE_COMPLETION_EMBED_H \
        -I$(top_builddir)/include/ \
        $(AM_CPPFLAGS)
img2sixel_CFLAGS = $(AM_CFLAGS) $(MAYBE_COVERAGE)
img2sixel_LDFLAGS = $(MAYBE_COVERAGE)
img2sixel_LDADD = $(top_builddir)/src/libsixel.la
if HAVE_ABORT_TRACE
img2sixel_LDADD += $(ABORTTRACE_LIBS)
endif
dist_man_MANS += $(srcdir)/img2sixel.1
dist_bashcompletion_DATA += $(srcdir)/shell-completion/bash/img2sixel
dist_zshcompletion_DATA += $(srcdir)/shell-completion/zsh/_img2sixel
endif

if WANT_SIXEL2PNG
bin_PROGRAMS += sixel2png
sixel2png_SOURCES = $(srcdir)/sixel2png.c \
                    $(srcdir)/cli.c \
                    $(srcdir)/cli.h \
                    $(srcdir)/malloc_stub.c \
                    $(srcdir)/malloc_stub.h \
                    $(srcdir)/getopt_stub.h \
                    $(srcdir)/aborttrace.c \
                    $(srcdir)/aborttrace.h
sixel2png_CPPFLAGS = -I$(top_builddir)/include/ $(AM_CPPFLAGS)
sixel2png_CFLAGS = $(AM_CFLAGS) $(MAYBE_COVERAGE)
sixel2png_LDFLAGS = $(MAYBE_COVERAGE)
sixel2png_LDADD = $(top_builddir)/src/libsixel.la
if HAVE_ABORT_TRACE
sixel2png_LDADD += $(ABORTTRACE_LIBS)
endif
dist_man_MANS += $(srcdir)/sixel2png.1
endif
