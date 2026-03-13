# SPDX-License-Identifier: MIT
#
# Copyright (c) 2026 libsixel developers. See `AUTHORS`.
#

srcdir = @srcdir@
top_srcdir = @top_srcdir@
top_builddir = @top_builddir@
builddir = @builddir@
VPATH = @srcdir@

FUZZ_CFLAGS = @FUZZ_CFLAGS@
FUZZ_LDFLAGS = @FUZZ_LDFLAGS@

if COND_FUZZ
noinst_PROGRAMS = fuzz-loader-builtin-libfuzzer
endif

EXTRA_DIST = \
    fuzz-loader-builtin-libfuzzer.c

fuzz_loader_builtin_libfuzzer_SOURCES = \
    $(srcdir)/fuzz-loader-builtin-libfuzzer.c

fuzz_loader_builtin_libfuzzer_CPPFLAGS = \
    -I$(top_builddir) \
    -I$(top_builddir)/include \
    -I$(top_srcdir) \
    -I$(top_srcdir)/include \
    -I$(top_srcdir)/src \
    $(AM_CPPFLAGS)

fuzz_loader_builtin_libfuzzer_CFLAGS = \
    $(CFLAGS) \
    $(AM_CFLAGS) \
    $(FUZZ_CFLAGS)

fuzz_loader_builtin_libfuzzer_LDFLAGS = \
    $(FUZZ_LDFLAGS)

fuzz_loader_builtin_libfuzzer_LDADD = \
    $(top_builddir)/src/libsixel.la \
    $(LIBS)

FUZZ_BUILTIN_CORPUS ?= $(top_srcdir)/images
FUZZ_BUILTIN_ARGS ?=

.PHONY: run-loader-builtin

run-loader-builtin: fuzz-loader-builtin-libfuzzer$(EXEEXT)
	@set -eu; \
	target="$(abs_builddir)/fuzz-loader-builtin-libfuzzer$(EXEEXT)"; \
	corpus="$(FUZZ_BUILTIN_CORPUS)"; \
	mkdir -p "$$corpus"; \
	echo "fuzz: running $$target $(FUZZ_BUILTIN_ARGS) $$corpus"; \
	exec "$$target" $(FUZZ_BUILTIN_ARGS) "$$corpus"

