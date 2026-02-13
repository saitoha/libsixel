DIST_SUBDIRS = $(SUBDIRS)

srcdir = @srcdir@
top_builddir = @top_builddir@
builddir = @builddir@
VPATH = @srcdir@

BUILT_SOURCES = completion_embed.h

SIXEL_SHEBANG_FILE = @SIXEL_SHEBANG_FILE@
SIXEL_SHEBANG_HELPER = $(top_srcdir)/build-aux/sixel-apply-shebang.sh

AMALGAMATION_SIXEL = $(top_builddir)/amalgamation/sixel.c

if WANT_AMALGAMATED_TOOLS
BUILT_SOURCES += $(AMALGAMATION_SIXEL)

# Keep this recipe tab-indented; GNU make requires tabs for commands.
$(AMALGAMATION_SIXEL): $(top_builddir)/amalgamation/Makefile
	$(AM_V_GEN)$(MAKE) -C $(top_builddir)/amalgamation sixel.c
$(AMALGAMATION_SIXEL): completion_embed.h
endif

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
if CFLAGS_HAVE_COVERAGE
MAYBE_COVERAGE = --coverage
endif

AMALGAMATION_TOOL_CPPFLAGS = $(AM_CPPFLAGS) \
        $(LIBCURL_CFLAGS) \
        $(WINHTTP_CFLAGS) \
        $(LIBPNG_CFLAGS) \
        $(LIBTIFF_CFLAGS) \
        $(LIBJPEG_CFLAGS) \
        $(LIBWEBP_CFLAGS) \
        $(GDK_PIXBUF_CFLAGS) \
        $(GD_CFLAGS) \
        $(WIC_CFLAGS) \
        $(COREGRAPHICS_CFLAGS) \
        $(QUICKLOOK_CFLAGS) \
        $(CLIPBOARD_CFLAGS) \
        @THREAD_CFLAGS@
if AMALGAMATION_NEEDS_OBJC
AMALGAMATION_TOOL_CPPFLAGS += -x objective-c
endif

AMALGAMATION_TOOL_LIBS = -lm \
        $(LIBCURL_LIBS) \
        $(WINHTTP_LIBS) \
        $(LIBPNG_LIBS) \
        $(LIBTIFF_LIBS) \
        $(LIBJPEG_LIBS) \
        $(LIBWEBP_LIBS) \
        $(GDK_PIXBUF_LIBS) \
        $(GD_LIBS) \
        $(WIC_LIBS) \
        $(COREGRAPHICS_LIBS) \
        $(QUICKLOOK_LIBS) \
        $(CLIPBOARD_LIBS) \
        @THREAD_LIBS@

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

# Keep this recipe tab-indented; GNU make requires tabs for commands.
completion_embed.h: \
	$(srcdir)/shell-completion/bash/img2sixel \
	$(srcdir)/shell-completion/zsh/_img2sixel \
	$(top_srcdir)/tools/gen_completion_embed.awk
	$(AM_V_GEN)$(AWK) -f $(top_srcdir)/tools/gen_completion_embed.awk \
	       $(srcdir)/shell-completion/bash/img2sixel \
	       $(srcdir)/shell-completion/zsh/_img2sixel > $@

if WANT_IMG2SIXEL
bin_PROGRAMS += img2sixel
if WANT_AMALGAMATED_TOOLS
img2sixel_SOURCES =
nodist_img2sixel_SOURCES = $(AMALGAMATION_SIXEL)
img2sixel_CPPFLAGS = $(AMALGAMATION_TOOL_CPPFLAGS) \
        -DBUILD_IMG2SIXEL -DHAVE_COMPLETION_EMBED_H
img2sixel_CFLAGS = $(CFLAGS) $(AM_CFLAGS) $(MAYBE_COVERAGE)
img2sixel_LDFLAGS = $(MAYBE_COVERAGE)
img2sixel_LDADD = $(AMALGAMATION_TOOL_LIBS)
else
img2sixel_SOURCES = $(srcdir)/img2sixel.c \
                    $(srcdir)/completion_utils.c \
                    $(srcdir)/completion_utils.h \
                    $(srcdir)/path.c \
                    $(srcdir)/path.h \
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
img2sixel_CPPFLAGS = -DBUILD_IMG2SIXEL -DHAVE_COMPLETION_EMBED_H \
        -I$(top_builddir)/include/ \
        $(AM_CPPFLAGS)
img2sixel_CFLAGS = $(CFLAGS) $(AM_CFLAGS) $(MAYBE_COVERAGE)
img2sixel_LDFLAGS = $(MAYBE_COVERAGE)
img2sixel_LDADD = $(top_builddir)/src/libsixel.la
if HAVE_ABORT_TRACE
img2sixel_LDADD += $(ABORTTRACE_LIBS)
endif
endif
dist_man_MANS += $(srcdir)/img2sixel.1
dist_bashcompletion_DATA += $(srcdir)/shell-completion/bash/img2sixel
dist_zshcompletion_DATA += $(srcdir)/shell-completion/zsh/_img2sixel
endif

if WANT_SIXEL2PNG
bin_PROGRAMS += sixel2png
sixel2png_CPPFLAGS = $(AM_CPPFLAGS)
if WANT_AMALGAMATED_TOOLS
sixel2png_SOURCES =
nodist_sixel2png_SOURCES = $(AMALGAMATION_SIXEL)
sixel2png_CPPFLAGS += $(AMALGAMATION_TOOL_CPPFLAGS) -DBUILD_SIXEL2PNG
sixel2png_CFLAGS = $(CFLAGS) $(AM_CFLAGS) $(MAYBE_COVERAGE)
sixel2png_LDFLAGS = $(MAYBE_COVERAGE)
sixel2png_LDADD = $(AMALGAMATION_TOOL_LIBS)
else
sixel2png_SOURCES = $(srcdir)/sixel2png.c \
                    $(srcdir)/cli.c \
                    $(srcdir)/cli.h \
                    $(srcdir)/malloc_stub.c \
                    $(srcdir)/malloc_stub.h \
                    $(srcdir)/getopt_stub.h \
                    $(srcdir)/aborttrace.c \
                    $(srcdir)/aborttrace.h
sixel2png_CPPFLAGS += -DBUILD_SIXEL2PNG -I$(top_builddir)/include/
sixel2png_CFLAGS = $(CFLAGS) $(AM_CFLAGS) $(MAYBE_COVERAGE)
sixel2png_LDFLAGS = $(MAYBE_COVERAGE)
sixel2png_LDADD = $(top_builddir)/src/libsixel.la
if HAVE_ABORT_TRACE
sixel2png_LDADD += $(ABORTTRACE_LIBS)
endif
endif
dist_man_MANS += $(srcdir)/sixel2png.1
endif


if WITH_SHEBANG_FILE
SIXEL_SHEBANG_TARGETS = $(bin_PROGRAMS)
SIXEL_SHEBANG_STAMP = converters-shebang.stamp

$(SIXEL_SHEBANG_STAMP): $(SIXEL_SHEBANG_TARGETS)
	@if test -n "$(SIXEL_SHEBANG_TARGETS)"; then \
		$(SIXEL_SHEBANG_HELPER) "$(SIXEL_SHEBANG_FILE)" \
			$(SIXEL_SHEBANG_TARGETS); \
	fi
	@touch $@

all-local: $(SIXEL_SHEBANG_STAMP)
CLEANFILES += $(SIXEL_SHEBANG_STAMP)
endif
