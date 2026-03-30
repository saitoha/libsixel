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
    fuzz-loader-builtin-libfuzzer.c \
    loader-builtin.dict

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
FUZZ_BUILTIN_FORMATS = png gif jpeg pnm sixel hdr psd pic bmp

.PHONY: run-loader-builtin run-loader-builtin-formats \
    $(FUZZ_BUILTIN_FORMATS:%=run-loader-builtin-%)

run-loader-builtin: fuzz-loader-builtin-libfuzzer$(EXEEXT)
	@set -eu; \
	target="$(abs_builddir)/fuzz-loader-builtin-libfuzzer$(EXEEXT)"; \
	corpus="$(FUZZ_BUILTIN_CORPUS)"; \
	mkdir -p "$$corpus"; \
	echo "fuzz: running $$target $(FUZZ_BUILTIN_ARGS) $$corpus"; \
	exec "$$target" $(FUZZ_BUILTIN_ARGS) "$$corpus"

run-loader-builtin-%: fuzz-loader-builtin-libfuzzer$(EXEEXT)
	@set -eu; \
	format="$*"; \
	target="$(abs_builddir)/fuzz-loader-builtin-libfuzzer$(EXEEXT)"; \
	corpus="$(FUZZ_BUILTIN_CORPUS)/$$format"; \
	mkdir -p "$$corpus"; \
	echo "fuzz[$$format]: running $$target $(FUZZ_BUILTIN_ARGS) $$corpus"; \
	FUZZ_BUILTIN_FORCE_FORMAT="$$format" \
		exec "$$target" $(FUZZ_BUILTIN_ARGS) "$$corpus"

run-loader-builtin-formats: fuzz-loader-builtin-libfuzzer$(EXEEXT)
	@set -eu; \
	for format in $(FUZZ_BUILTIN_FORMATS); do \
		$(MAKE) $(AM_MAKEFLAGS) run-loader-builtin-$$format \
			FUZZ_BUILTIN_CORPUS='$(FUZZ_BUILTIN_CORPUS)' \
			FUZZ_BUILTIN_ARGS='$(FUZZ_BUILTIN_ARGS)' || exit $$?; \
	done
