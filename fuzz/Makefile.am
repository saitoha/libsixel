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
    png-icc-gamma \
    png-plte-trns \
    jpeg-icc \
    pnm-ascii \
    psd-resource \
    gif-block \
    gif-lzw-codes \
    sixel-stream \
    hdr-header \
    pic-structure \
    bmp-dib \
    bmp-rle-bitfields \
    mapfile-spec \
    mapfile-palette \
    option-arglist \
    loader-order

FUZZ_STRUCT_PUSH_SET = \
    png-chunk \
    png-apng \
    jpeg-icc \
    pnm-ascii \
    psd-resource

FUZZ_STRUCT_FULL_SET = $(FUZZ_STRUCT_TARGETS)

if COND_FUZZ
noinst_PROGRAMS = \
    fuzz-encoder-encode-bytes-libfuzzer \
    fuzz-helper-write-image-file-libfuzzer \
    fuzz-loader-builtin-libfuzzer \
    fuzz-loader-builtin-struct-png-chunk-libfuzzer \
    fuzz-loader-builtin-struct-png-apng-libfuzzer \
    fuzz-loader-builtin-struct-png-icc-gamma-libfuzzer \
    fuzz-loader-builtin-struct-png-plte-trns-libfuzzer \
    fuzz-loader-builtin-struct-jpeg-icc-libfuzzer \
    fuzz-loader-builtin-struct-pnm-ascii-libfuzzer \
    fuzz-loader-builtin-struct-psd-resource-libfuzzer \
    fuzz-loader-builtin-struct-gif-block-libfuzzer \
    fuzz-loader-builtin-struct-gif-lzw-codes-libfuzzer \
    fuzz-loader-builtin-struct-sixel-stream-libfuzzer \
    fuzz-loader-builtin-struct-hdr-header-libfuzzer \
    fuzz-loader-builtin-struct-pic-structure-libfuzzer \
    fuzz-loader-builtin-struct-bmp-dib-libfuzzer \
    fuzz-loader-builtin-struct-bmp-rle-bitfields-libfuzzer \
    fuzz-loader-builtin-struct-mapfile-spec-libfuzzer \
    fuzz-loader-builtin-struct-mapfile-palette-libfuzzer \
    fuzz-loader-builtin-struct-option-arglist-libfuzzer \
    fuzz-loader-builtin-struct-loader-order-libfuzzer
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
    fuzz-img2sixel-cli-parser-afl-persistent.c \
    fuzz-img2sixel-cli-boundary-afl-persistent.c \
    fuzz-img2sixel-afl-persistent.c \
    corpus/img2sixel-cli/seed-lzw-cs0.gif \
    corpus/img2sixel-cli/seed-min.hdr \
    corpus/img2sixel-cli/seed-min.psd \
    corpus/img2sixel-cli/seed-min.six \
    corpus/img2sixel-cli/seed-overflow.p3 \
    corpus/img2sixel-cli/seed-pal8.bmp \
    corpus/img2sixel-cli/seed-progressive.jpg \
    corpus/img2sixel-cli/seed-rgba.png \
    corpus/img2sixel-cli/seed-short.bin \
    corpus/img2sixel-cli/seed-snake64.pbm \
    corpus/img2sixel-cli/seed-static.gif \
    corpus/img2sixel-cli/seed-truncated.pic \
    fuzz-encoder-encode-bytes-libfuzzer.c \
    fuzz-helper-write-image-file-libfuzzer.c \
    fuzz-loader-builtin-libfuzzer.c \
    fuzz-loader-component-libfuzzer.c \
    fuzz-loader-builtin-struct-common.h \
    fuzz-loader-builtin-struct-runtime.c \
    fuzz-loader-builtin-struct-png-chunk-libfuzzer.c \
    fuzz-loader-builtin-struct-png-apng-libfuzzer.c \
    fuzz-loader-builtin-struct-png-icc-gamma-libfuzzer.c \
    fuzz-loader-builtin-struct-png-plte-trns-libfuzzer.c \
    fuzz-loader-builtin-struct-jpeg-icc-libfuzzer.c \
    fuzz-loader-builtin-struct-pnm-ascii-libfuzzer.c \
    fuzz-loader-builtin-struct-psd-resource-libfuzzer.c \
    fuzz-loader-builtin-struct-gif-block-libfuzzer.c \
    fuzz-loader-builtin-struct-gif-lzw-codes-libfuzzer.c \
    fuzz-loader-builtin-struct-sixel-stream-libfuzzer.c \
    fuzz-loader-builtin-struct-hdr-header-libfuzzer.c \
    fuzz-loader-builtin-struct-pic-structure-libfuzzer.c \
    fuzz-loader-builtin-struct-bmp-dib-libfuzzer.c \
    fuzz-loader-builtin-struct-bmp-rle-bitfields-libfuzzer.c \
    fuzz-loader-builtin-struct-mapfile-spec-libfuzzer.c \
    fuzz-loader-builtin-struct-mapfile-palette-libfuzzer.c \
    fuzz-loader-builtin-struct-option-arglist-libfuzzer.c \
    fuzz-loader-builtin-struct-loader-order-libfuzzer.c \
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

fuzz_encoder_encode_bytes_libfuzzer_SOURCES = \
    $(srcdir)/fuzz-encoder-encode-bytes-libfuzzer.c
fuzz_encoder_encode_bytes_libfuzzer_CPPFLAGS = $(FUZZ_COMMON_CPPFLAGS)
fuzz_encoder_encode_bytes_libfuzzer_CFLAGS = $(FUZZ_COMMON_CFLAGS)
fuzz_encoder_encode_bytes_libfuzzer_LDFLAGS = $(FUZZ_COMMON_LINK_FLAGS)
fuzz_encoder_encode_bytes_libfuzzer_LDADD = $(FUZZ_COMMON_LINK_LIBS)

fuzz_helper_write_image_file_libfuzzer_SOURCES = \
    $(srcdir)/fuzz-helper-write-image-file-libfuzzer.c
fuzz_helper_write_image_file_libfuzzer_CPPFLAGS = $(FUZZ_COMMON_CPPFLAGS)
fuzz_helper_write_image_file_libfuzzer_CFLAGS = $(FUZZ_COMMON_CFLAGS)
fuzz_helper_write_image_file_libfuzzer_LDFLAGS = $(FUZZ_COMMON_LINK_FLAGS)
fuzz_helper_write_image_file_libfuzzer_LDADD = $(FUZZ_COMMON_LINK_LIBS)

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

fuzz_loader_builtin_struct_png_icc_gamma_libfuzzer_SOURCES = \
    $(srcdir)/fuzz-loader-builtin-struct-png-icc-gamma-libfuzzer.c \
    $(srcdir)/fuzz-loader-builtin-struct-runtime.c
fuzz_loader_builtin_struct_png_icc_gamma_libfuzzer_CPPFLAGS = $(FUZZ_COMMON_CPPFLAGS)
fuzz_loader_builtin_struct_png_icc_gamma_libfuzzer_CFLAGS = $(FUZZ_COMMON_CFLAGS)
fuzz_loader_builtin_struct_png_icc_gamma_libfuzzer_LDFLAGS = $(FUZZ_COMMON_LINK_FLAGS)
fuzz_loader_builtin_struct_png_icc_gamma_libfuzzer_LDADD = $(FUZZ_COMMON_LINK_LIBS)

fuzz_loader_builtin_struct_png_plte_trns_libfuzzer_SOURCES = \
    $(srcdir)/fuzz-loader-builtin-struct-png-plte-trns-libfuzzer.c \
    $(srcdir)/fuzz-loader-builtin-struct-runtime.c
fuzz_loader_builtin_struct_png_plte_trns_libfuzzer_CPPFLAGS = $(FUZZ_COMMON_CPPFLAGS)
fuzz_loader_builtin_struct_png_plte_trns_libfuzzer_CFLAGS = $(FUZZ_COMMON_CFLAGS)
fuzz_loader_builtin_struct_png_plte_trns_libfuzzer_LDFLAGS = $(FUZZ_COMMON_LINK_FLAGS)
fuzz_loader_builtin_struct_png_plte_trns_libfuzzer_LDADD = $(FUZZ_COMMON_LINK_LIBS)

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

fuzz_loader_builtin_struct_gif_lzw_codes_libfuzzer_SOURCES = \
    $(srcdir)/fuzz-loader-builtin-struct-gif-lzw-codes-libfuzzer.c \
    $(srcdir)/fuzz-loader-builtin-struct-runtime.c
fuzz_loader_builtin_struct_gif_lzw_codes_libfuzzer_CPPFLAGS = $(FUZZ_COMMON_CPPFLAGS)
fuzz_loader_builtin_struct_gif_lzw_codes_libfuzzer_CFLAGS = $(FUZZ_COMMON_CFLAGS)
fuzz_loader_builtin_struct_gif_lzw_codes_libfuzzer_LDFLAGS = $(FUZZ_COMMON_LINK_FLAGS)
fuzz_loader_builtin_struct_gif_lzw_codes_libfuzzer_LDADD = $(FUZZ_COMMON_LINK_LIBS)

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

fuzz_loader_builtin_struct_bmp_rle_bitfields_libfuzzer_SOURCES = \
    $(srcdir)/fuzz-loader-builtin-struct-bmp-rle-bitfields-libfuzzer.c \
    $(srcdir)/fuzz-loader-builtin-struct-runtime.c
fuzz_loader_builtin_struct_bmp_rle_bitfields_libfuzzer_CPPFLAGS = $(FUZZ_COMMON_CPPFLAGS)
fuzz_loader_builtin_struct_bmp_rle_bitfields_libfuzzer_CFLAGS = $(FUZZ_COMMON_CFLAGS)
fuzz_loader_builtin_struct_bmp_rle_bitfields_libfuzzer_LDFLAGS = $(FUZZ_COMMON_LINK_FLAGS)
fuzz_loader_builtin_struct_bmp_rle_bitfields_libfuzzer_LDADD = $(FUZZ_COMMON_LINK_LIBS)

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

fuzz_loader_builtin_struct_option_arglist_libfuzzer_SOURCES = \
    $(srcdir)/fuzz-loader-builtin-struct-option-arglist-libfuzzer.c
fuzz_loader_builtin_struct_option_arglist_libfuzzer_CPPFLAGS = $(FUZZ_COMMON_CPPFLAGS)
fuzz_loader_builtin_struct_option_arglist_libfuzzer_CFLAGS = $(FUZZ_COMMON_CFLAGS)
fuzz_loader_builtin_struct_option_arglist_libfuzzer_LDFLAGS = $(FUZZ_COMMON_LINK_FLAGS)
fuzz_loader_builtin_struct_option_arglist_libfuzzer_LDADD = $(FUZZ_COMMON_LINK_LIBS)

fuzz_loader_builtin_struct_loader_order_libfuzzer_SOURCES = \
    $(srcdir)/fuzz-loader-builtin-struct-loader-order-libfuzzer.c
fuzz_loader_builtin_struct_loader_order_libfuzzer_CPPFLAGS = $(FUZZ_COMMON_CPPFLAGS)
fuzz_loader_builtin_struct_loader_order_libfuzzer_CFLAGS = $(FUZZ_COMMON_CFLAGS)
fuzz_loader_builtin_struct_loader_order_libfuzzer_LDFLAGS = $(FUZZ_COMMON_LINK_FLAGS)
fuzz_loader_builtin_struct_loader_order_libfuzzer_LDADD = $(FUZZ_COMMON_LINK_LIBS)

if HAVE_COREGRAPHICS
fuzz_loader_coregraphics_libfuzzer_SOURCES = \
    $(srcdir)/fuzz-loader-component-libfuzzer.c
fuzz_loader_coregraphics_libfuzzer_CPPFLAGS = \
    $(FUZZ_COMMON_CPPFLAGS) \
    -DFUZZ_LOADER_COMPONENT_NAME=\"coregraphics\"
fuzz_loader_coregraphics_libfuzzer_CFLAGS = $(FUZZ_COMMON_CFLAGS)
fuzz_loader_coregraphics_libfuzzer_LDFLAGS = $(FUZZ_COMMON_LINK_FLAGS)
fuzz_loader_coregraphics_libfuzzer_LDADD = $(FUZZ_COMMON_LINK_LIBS)
endif

if HAVE_WIC
fuzz_loader_wic_libfuzzer_SOURCES = \
    $(srcdir)/fuzz-loader-component-libfuzzer.c
fuzz_loader_wic_libfuzzer_CPPFLAGS = \
    $(FUZZ_COMMON_CPPFLAGS) \
    -DFUZZ_LOADER_COMPONENT_NAME=\"wic\"
fuzz_loader_wic_libfuzzer_CFLAGS = $(FUZZ_COMMON_CFLAGS)
fuzz_loader_wic_libfuzzer_LDFLAGS = $(FUZZ_COMMON_LINK_FLAGS)
fuzz_loader_wic_libfuzzer_LDADD = $(FUZZ_COMMON_LINK_LIBS)
endif

FUZZ_BUILTIN_CORPUS ?= $(top_srcdir)/images
FUZZ_BUILTIN_ARGS ?=
FUZZ_ENCODE_BYTES_CORPUS ?= $(FUZZ_BUILTIN_CORPUS)/encode-bytes
FUZZ_ENCODE_BYTES_ARGS ?= $(FUZZ_BUILTIN_ARGS)
FUZZ_WRITE_IMAGE_CORPUS ?= $(FUZZ_BUILTIN_CORPUS)/write-image-file
FUZZ_WRITE_IMAGE_ARGS ?= $(FUZZ_BUILTIN_ARGS)
FUZZ_BUILTIN_FORMATS = png gif jpeg pnm sixel hdr psd pic bmp webp
FUZZ_STRUCT_CORPUS ?= $(FUZZ_BUILTIN_CORPUS)/struct
FUZZ_STRUCT_ARGS ?= $(FUZZ_BUILTIN_ARGS)
FUZZ_COMPONENT_CORPUS ?= $(FUZZ_BUILTIN_CORPUS)/component
FUZZ_COMPONENT_ARGS ?= $(FUZZ_BUILTIN_ARGS)

.PHONY: run-encoder-encode-bytes run-helper-write-image-file \
    run-loader-builtin run-loader-builtin-formats \
    run-loader-coregraphics run-loader-wic \
    run-struct-push-set run-struct-full-set

run-encoder-encode-bytes: fuzz-encoder-encode-bytes-libfuzzer$(EXEEXT)
	@set -eu; \
	target="$(abs_builddir)/fuzz-encoder-encode-bytes-libfuzzer$(EXEEXT)"; \
	corpus="$(FUZZ_ENCODE_BYTES_CORPUS)"; \
	mkdir -p "$$corpus"; \
	echo "fuzz[encode-bytes]: running $$target $(FUZZ_ENCODE_BYTES_ARGS) $$corpus"; \
	exec "$$target" $(FUZZ_ENCODE_BYTES_ARGS) "$$corpus"

run-helper-write-image-file: fuzz-helper-write-image-file-libfuzzer$(EXEEXT)
	@set -eu; \
	target="$(abs_builddir)/fuzz-helper-write-image-file-libfuzzer$(EXEEXT)"; \
	corpus="$(FUZZ_WRITE_IMAGE_CORPUS)"; \
	mkdir -p "$$corpus"; \
	echo "fuzz[write-image-file]: running $$target $(FUZZ_WRITE_IMAGE_ARGS) $$corpus"; \
	exec "$$target" $(FUZZ_WRITE_IMAGE_ARGS) "$$corpus"

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
