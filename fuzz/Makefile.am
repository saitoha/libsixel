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

FUZZ_STRUCT_TARGETS = \
    png-chunk \
    png-apng \
    jpeg-icc \
    pnm-ascii \
    psd-resource \
    gif-block \
    sixel-stream \
    hdr-header \
    pic-structure \
    bmp-dib \
    mapfile-spec \
    mapfile-palette

FUZZ_STRUCT_PUSH_SET = \
    png-chunk \
    png-apng \
    jpeg-icc \
    pnm-ascii \
    psd-resource

FUZZ_STRUCT_FULL_SET = $(FUZZ_STRUCT_TARGETS)

if COND_FUZZ
noinst_PROGRAMS = \
    fuzz-loader-builtin-libfuzzer \
    fuzz-loader-builtin-struct-png-chunk-libfuzzer \
    fuzz-loader-builtin-struct-png-apng-libfuzzer \
    fuzz-loader-builtin-struct-jpeg-icc-libfuzzer \
    fuzz-loader-builtin-struct-pnm-ascii-libfuzzer \
    fuzz-loader-builtin-struct-psd-resource-libfuzzer \
    fuzz-loader-builtin-struct-gif-block-libfuzzer \
    fuzz-loader-builtin-struct-sixel-stream-libfuzzer \
    fuzz-loader-builtin-struct-hdr-header-libfuzzer \
    fuzz-loader-builtin-struct-pic-structure-libfuzzer \
    fuzz-loader-builtin-struct-bmp-dib-libfuzzer \
    fuzz-loader-builtin-struct-mapfile-spec-libfuzzer \
    fuzz-loader-builtin-struct-mapfile-palette-libfuzzer
if HAVE_COREGRAPHICS
noinst_PROGRAMS += \
    fuzz-loader-coregraphics-libfuzzer
endif
if HAVE_WIC
noinst_PROGRAMS += \
    fuzz-loader-wic-libfuzzer
endif
endif

EXTRA_DIST = \
    fuzz-loader-builtin-libfuzzer.c \
    fuzz-loader-component-libfuzzer.c \
    fuzz-loader-builtin-struct-common.h \
    fuzz-loader-builtin-struct-runtime.c \
    fuzz-loader-builtin-struct-png-chunk-libfuzzer.c \
    fuzz-loader-builtin-struct-png-apng-libfuzzer.c \
    fuzz-loader-builtin-struct-jpeg-icc-libfuzzer.c \
    fuzz-loader-builtin-struct-pnm-ascii-libfuzzer.c \
    fuzz-loader-builtin-struct-psd-resource-libfuzzer.c \
    fuzz-loader-builtin-struct-gif-block-libfuzzer.c \
    fuzz-loader-builtin-struct-sixel-stream-libfuzzer.c \
    fuzz-loader-builtin-struct-hdr-header-libfuzzer.c \
    fuzz-loader-builtin-struct-pic-structure-libfuzzer.c \
    fuzz-loader-builtin-struct-bmp-dib-libfuzzer.c \
    fuzz-loader-builtin-struct-mapfile-spec-libfuzzer.c \
    fuzz-loader-builtin-struct-mapfile-palette-libfuzzer.c \
    loader-builtin.dict

FUZZ_COMMON_CPPFLAGS = \
    -I$(top_builddir) \
    -I$(top_builddir)/include \
    -I$(top_srcdir) \
    -I$(top_srcdir)/include \
    -I$(top_srcdir)/src \
    $(AM_CPPFLAGS)

FUZZ_COMMON_CFLAGS = \
    $(CFLAGS) \
    $(AM_CFLAGS) \
    $(FUZZ_CFLAGS)

FUZZ_COMMON_LINK_FLAGS = \
    $(FUZZ_LDFLAGS)

FUZZ_COMMON_LINK_LIBS = \
    $(top_builddir)/src/libsixel.la \
    $(LIBS)

fuzz_loader_builtin_libfuzzer_SOURCES = \
    $(srcdir)/fuzz-loader-builtin-libfuzzer.c

fuzz_loader_builtin_libfuzzer_CPPFLAGS = $(FUZZ_COMMON_CPPFLAGS)
fuzz_loader_builtin_libfuzzer_CFLAGS = $(FUZZ_COMMON_CFLAGS)
fuzz_loader_builtin_libfuzzer_LDFLAGS = $(FUZZ_COMMON_LINK_FLAGS)
fuzz_loader_builtin_libfuzzer_LDADD = $(FUZZ_COMMON_LINK_LIBS)

fuzz_loader_builtin_struct_png_chunk_libfuzzer_SOURCES = \
    $(srcdir)/fuzz-loader-builtin-struct-png-chunk-libfuzzer.c \
    $(srcdir)/fuzz-loader-builtin-struct-runtime.c
fuzz_loader_builtin_struct_png_chunk_libfuzzer_CPPFLAGS = $(FUZZ_COMMON_CPPFLAGS)
fuzz_loader_builtin_struct_png_chunk_libfuzzer_CFLAGS = $(FUZZ_COMMON_CFLAGS)
fuzz_loader_builtin_struct_png_chunk_libfuzzer_LDFLAGS = $(FUZZ_COMMON_LINK_FLAGS)
fuzz_loader_builtin_struct_png_chunk_libfuzzer_LDADD = $(FUZZ_COMMON_LINK_LIBS)

fuzz_loader_builtin_struct_png_apng_libfuzzer_SOURCES = \
    $(srcdir)/fuzz-loader-builtin-struct-png-apng-libfuzzer.c \
    $(srcdir)/fuzz-loader-builtin-struct-runtime.c
fuzz_loader_builtin_struct_png_apng_libfuzzer_CPPFLAGS = $(FUZZ_COMMON_CPPFLAGS)
fuzz_loader_builtin_struct_png_apng_libfuzzer_CFLAGS = $(FUZZ_COMMON_CFLAGS)
fuzz_loader_builtin_struct_png_apng_libfuzzer_LDFLAGS = $(FUZZ_COMMON_LINK_FLAGS)
fuzz_loader_builtin_struct_png_apng_libfuzzer_LDADD = $(FUZZ_COMMON_LINK_LIBS)

fuzz_loader_builtin_struct_jpeg_icc_libfuzzer_SOURCES = \
    $(srcdir)/fuzz-loader-builtin-struct-jpeg-icc-libfuzzer.c \
    $(srcdir)/fuzz-loader-builtin-struct-runtime.c
fuzz_loader_builtin_struct_jpeg_icc_libfuzzer_CPPFLAGS = $(FUZZ_COMMON_CPPFLAGS)
fuzz_loader_builtin_struct_jpeg_icc_libfuzzer_CFLAGS = $(FUZZ_COMMON_CFLAGS)
fuzz_loader_builtin_struct_jpeg_icc_libfuzzer_LDFLAGS = $(FUZZ_COMMON_LINK_FLAGS)
fuzz_loader_builtin_struct_jpeg_icc_libfuzzer_LDADD = $(FUZZ_COMMON_LINK_LIBS)

fuzz_loader_builtin_struct_pnm_ascii_libfuzzer_SOURCES = \
    $(srcdir)/fuzz-loader-builtin-struct-pnm-ascii-libfuzzer.c \
    $(srcdir)/fuzz-loader-builtin-struct-runtime.c
fuzz_loader_builtin_struct_pnm_ascii_libfuzzer_CPPFLAGS = $(FUZZ_COMMON_CPPFLAGS)
fuzz_loader_builtin_struct_pnm_ascii_libfuzzer_CFLAGS = $(FUZZ_COMMON_CFLAGS)
fuzz_loader_builtin_struct_pnm_ascii_libfuzzer_LDFLAGS = $(FUZZ_COMMON_LINK_FLAGS)
fuzz_loader_builtin_struct_pnm_ascii_libfuzzer_LDADD = $(FUZZ_COMMON_LINK_LIBS)

fuzz_loader_builtin_struct_psd_resource_libfuzzer_SOURCES = \
    $(srcdir)/fuzz-loader-builtin-struct-psd-resource-libfuzzer.c \
    $(srcdir)/fuzz-loader-builtin-struct-runtime.c
fuzz_loader_builtin_struct_psd_resource_libfuzzer_CPPFLAGS = $(FUZZ_COMMON_CPPFLAGS)
fuzz_loader_builtin_struct_psd_resource_libfuzzer_CFLAGS = $(FUZZ_COMMON_CFLAGS)
fuzz_loader_builtin_struct_psd_resource_libfuzzer_LDFLAGS = $(FUZZ_COMMON_LINK_FLAGS)
fuzz_loader_builtin_struct_psd_resource_libfuzzer_LDADD = $(FUZZ_COMMON_LINK_LIBS)

fuzz_loader_builtin_struct_gif_block_libfuzzer_SOURCES = \
    $(srcdir)/fuzz-loader-builtin-struct-gif-block-libfuzzer.c \
    $(srcdir)/fuzz-loader-builtin-struct-runtime.c
fuzz_loader_builtin_struct_gif_block_libfuzzer_CPPFLAGS = $(FUZZ_COMMON_CPPFLAGS)
fuzz_loader_builtin_struct_gif_block_libfuzzer_CFLAGS = $(FUZZ_COMMON_CFLAGS)
fuzz_loader_builtin_struct_gif_block_libfuzzer_LDFLAGS = $(FUZZ_COMMON_LINK_FLAGS)
fuzz_loader_builtin_struct_gif_block_libfuzzer_LDADD = $(FUZZ_COMMON_LINK_LIBS)

fuzz_loader_builtin_struct_sixel_stream_libfuzzer_SOURCES = \
    $(srcdir)/fuzz-loader-builtin-struct-sixel-stream-libfuzzer.c \
    $(srcdir)/fuzz-loader-builtin-struct-runtime.c
fuzz_loader_builtin_struct_sixel_stream_libfuzzer_CPPFLAGS = $(FUZZ_COMMON_CPPFLAGS)
fuzz_loader_builtin_struct_sixel_stream_libfuzzer_CFLAGS = $(FUZZ_COMMON_CFLAGS)
fuzz_loader_builtin_struct_sixel_stream_libfuzzer_LDFLAGS = $(FUZZ_COMMON_LINK_FLAGS)
fuzz_loader_builtin_struct_sixel_stream_libfuzzer_LDADD = $(FUZZ_COMMON_LINK_LIBS)

fuzz_loader_builtin_struct_hdr_header_libfuzzer_SOURCES = \
    $(srcdir)/fuzz-loader-builtin-struct-hdr-header-libfuzzer.c \
    $(srcdir)/fuzz-loader-builtin-struct-runtime.c
fuzz_loader_builtin_struct_hdr_header_libfuzzer_CPPFLAGS = $(FUZZ_COMMON_CPPFLAGS)
fuzz_loader_builtin_struct_hdr_header_libfuzzer_CFLAGS = $(FUZZ_COMMON_CFLAGS)
fuzz_loader_builtin_struct_hdr_header_libfuzzer_LDFLAGS = $(FUZZ_COMMON_LINK_FLAGS)
fuzz_loader_builtin_struct_hdr_header_libfuzzer_LDADD = $(FUZZ_COMMON_LINK_LIBS)

fuzz_loader_builtin_struct_pic_structure_libfuzzer_SOURCES = \
    $(srcdir)/fuzz-loader-builtin-struct-pic-structure-libfuzzer.c \
    $(srcdir)/fuzz-loader-builtin-struct-runtime.c
fuzz_loader_builtin_struct_pic_structure_libfuzzer_CPPFLAGS = $(FUZZ_COMMON_CPPFLAGS)
fuzz_loader_builtin_struct_pic_structure_libfuzzer_CFLAGS = $(FUZZ_COMMON_CFLAGS)
fuzz_loader_builtin_struct_pic_structure_libfuzzer_LDFLAGS = $(FUZZ_COMMON_LINK_FLAGS)
fuzz_loader_builtin_struct_pic_structure_libfuzzer_LDADD = $(FUZZ_COMMON_LINK_LIBS)

fuzz_loader_builtin_struct_bmp_dib_libfuzzer_SOURCES = \
    $(srcdir)/fuzz-loader-builtin-struct-bmp-dib-libfuzzer.c \
    $(srcdir)/fuzz-loader-builtin-struct-runtime.c
fuzz_loader_builtin_struct_bmp_dib_libfuzzer_CPPFLAGS = $(FUZZ_COMMON_CPPFLAGS)
fuzz_loader_builtin_struct_bmp_dib_libfuzzer_CFLAGS = $(FUZZ_COMMON_CFLAGS)
fuzz_loader_builtin_struct_bmp_dib_libfuzzer_LDFLAGS = $(FUZZ_COMMON_LINK_FLAGS)
fuzz_loader_builtin_struct_bmp_dib_libfuzzer_LDADD = $(FUZZ_COMMON_LINK_LIBS)

fuzz_loader_builtin_struct_mapfile_spec_libfuzzer_SOURCES = \
    $(srcdir)/fuzz-loader-builtin-struct-mapfile-spec-libfuzzer.c
fuzz_loader_builtin_struct_mapfile_spec_libfuzzer_CPPFLAGS = $(FUZZ_COMMON_CPPFLAGS)
fuzz_loader_builtin_struct_mapfile_spec_libfuzzer_CFLAGS = $(FUZZ_COMMON_CFLAGS)
fuzz_loader_builtin_struct_mapfile_spec_libfuzzer_LDFLAGS = $(FUZZ_COMMON_LINK_FLAGS)
fuzz_loader_builtin_struct_mapfile_spec_libfuzzer_LDADD = $(FUZZ_COMMON_LINK_LIBS)

fuzz_loader_builtin_struct_mapfile_palette_libfuzzer_SOURCES = \
    $(srcdir)/fuzz-loader-builtin-struct-mapfile-palette-libfuzzer.c
fuzz_loader_builtin_struct_mapfile_palette_libfuzzer_CPPFLAGS = $(FUZZ_COMMON_CPPFLAGS)
fuzz_loader_builtin_struct_mapfile_palette_libfuzzer_CFLAGS = $(FUZZ_COMMON_CFLAGS)
fuzz_loader_builtin_struct_mapfile_palette_libfuzzer_LDFLAGS = $(FUZZ_COMMON_LINK_FLAGS)
fuzz_loader_builtin_struct_mapfile_palette_libfuzzer_LDADD = $(FUZZ_COMMON_LINK_LIBS)

if HAVE_COREGRAPHICS
fuzz_loader_coregraphics_libfuzzer_SOURCES = \
    $(srcdir)/fuzz-loader-component-libfuzzer.c \
    $(srcdir)/fuzz-loader-builtin-struct-runtime.c
fuzz_loader_coregraphics_libfuzzer_CPPFLAGS = \
    $(FUZZ_COMMON_CPPFLAGS) \
    -DFUZZ_LOADER_COMPONENT_NAME=\"coregraphics\"
fuzz_loader_coregraphics_libfuzzer_CFLAGS = $(FUZZ_COMMON_CFLAGS)
fuzz_loader_coregraphics_libfuzzer_LDFLAGS = $(FUZZ_COMMON_LINK_FLAGS)
fuzz_loader_coregraphics_libfuzzer_LDADD = $(FUZZ_COMMON_LINK_LIBS)
endif

if HAVE_WIC
fuzz_loader_wic_libfuzzer_SOURCES = \
    $(srcdir)/fuzz-loader-component-libfuzzer.c \
    $(srcdir)/fuzz-loader-builtin-struct-runtime.c
fuzz_loader_wic_libfuzzer_CPPFLAGS = \
    $(FUZZ_COMMON_CPPFLAGS) \
    -DFUZZ_LOADER_COMPONENT_NAME=\"wic\"
fuzz_loader_wic_libfuzzer_CFLAGS = $(FUZZ_COMMON_CFLAGS)
fuzz_loader_wic_libfuzzer_LDFLAGS = $(FUZZ_COMMON_LINK_FLAGS)
fuzz_loader_wic_libfuzzer_LDADD = $(FUZZ_COMMON_LINK_LIBS)
endif

FUZZ_BUILTIN_CORPUS ?= $(top_srcdir)/images
FUZZ_BUILTIN_ARGS ?=
FUZZ_BUILTIN_FORMATS = png gif jpeg pnm sixel hdr psd pic bmp
FUZZ_STRUCT_CORPUS ?= $(FUZZ_BUILTIN_CORPUS)/struct
FUZZ_STRUCT_ARGS ?= $(FUZZ_BUILTIN_ARGS)
FUZZ_COMPONENT_CORPUS ?= $(FUZZ_BUILTIN_CORPUS)/component
FUZZ_COMPONENT_ARGS ?= $(FUZZ_BUILTIN_ARGS)

.PHONY: run-loader-builtin run-loader-builtin-formats \
    run-loader-coregraphics run-loader-wic \
    run-struct-push-set run-struct-full-set

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

if HAVE_COREGRAPHICS
run-loader-coregraphics: fuzz-loader-coregraphics-libfuzzer$(EXEEXT)
	@set -eu; \
	target="$(abs_builddir)/fuzz-loader-coregraphics-libfuzzer$(EXEEXT)"; \
	corpus="$(FUZZ_COMPONENT_CORPUS)/coregraphics"; \
	mkdir -p "$$corpus"; \
	echo "fuzz[loader:coregraphics]: running $$target $(FUZZ_COMPONENT_ARGS) $$corpus"; \
	exec "$$target" $(FUZZ_COMPONENT_ARGS) "$$corpus"
else
run-loader-coregraphics:
	@echo "error: coregraphics loader fuzz harness is unavailable in this build" >&2; \
	exit 1
endif

if HAVE_WIC
run-loader-wic: fuzz-loader-wic-libfuzzer$(EXEEXT)
	@set -eu; \
	target="$(abs_builddir)/fuzz-loader-wic-libfuzzer$(EXEEXT)"; \
	corpus="$(FUZZ_COMPONENT_CORPUS)/wic"; \
	mkdir -p "$$corpus"; \
	echo "fuzz[loader:wic]: running $$target $(FUZZ_COMPONENT_ARGS) $$corpus"; \
	exec "$$target" $(FUZZ_COMPONENT_ARGS) "$$corpus"
else
run-loader-wic:
	@echo "error: wic loader fuzz harness is unavailable in this build" >&2; \
	exit 1
endif

run-struct-%: fuzz-loader-builtin-struct-%-libfuzzer$(EXEEXT)
	@set -eu; \
	name="$*"; \
	target="$(abs_builddir)/fuzz-loader-builtin-struct-$$name-libfuzzer$(EXEEXT)"; \
	corpus="$(FUZZ_STRUCT_CORPUS)/$$name"; \
	mkdir -p "$$corpus"; \
	echo "fuzz[struct:$$name]: running $$target $(FUZZ_STRUCT_ARGS) $$corpus"; \
	exec "$$target" $(FUZZ_STRUCT_ARGS) "$$corpus"

run-struct-push-set:
	@set -eu; \
	for name in $(FUZZ_STRUCT_PUSH_SET); do \
		$(MAKE) $(AM_MAKEFLAGS) run-struct-$$name \
			FUZZ_STRUCT_CORPUS='$(FUZZ_STRUCT_CORPUS)' \
			FUZZ_STRUCT_ARGS='$(FUZZ_STRUCT_ARGS)' || exit $$?; \
	done

run-struct-full-set:
	@set -eu; \
	for name in $(FUZZ_STRUCT_FULL_SET); do \
		$(MAKE) $(AM_MAKEFLAGS) run-struct-$$name \
			FUZZ_STRUCT_CORPUS='$(FUZZ_STRUCT_CORPUS)' \
			FUZZ_STRUCT_ARGS='$(FUZZ_STRUCT_ARGS)' || exit $$?; \
	done
