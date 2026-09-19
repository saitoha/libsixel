/* SPDX-License-Identifier: MIT */
/* Observe the actual adapter after libtiff's decoded RGB8 boundary. The
 * original YCbCr conversion and the following ICC arithmetic have separate
 * independent selected-sample oracles; this is not a new source decoder. */
#include "config.h"
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "src/cms.h"
#include "src/frame-private.h"
#include "src/loader-libtiff.h"
#include "src/loader-common.h"
#include "src/chunk-factory.h"
#include "src/chunk-view.h"
#include "tiff_ycbcr_test_common.h"
#if HAVE_LIBTIFF
#include <tiffio.h>
#include "tests/data/color-numeric/tiff-legacy.h"
#include "tests/data/color-numeric/tiff-ycbcr.h"

static struct {
    int ok, engine, fault, target, armed, managed, mismatch;
    int reads, calls, applies, injected, real_pixels, live, phases;
    float native[18], expected[18];
} yc;

static void *
yc_malloc(size_t bytes)
{
    void *p;

    if (yc.armed && yc.fault == 2 && bytes == 18u * sizeof(float)) {
        ++yc.injected;
        return NULL;
    }
    p = malloc(bytes);
    yc.live += p != NULL;
    return p;
}

static void *
yc_calloc(size_t count, size_t size)
{
    void *p;

    p = calloc(count, size);
    yc.live += p != NULL;
    return p;
}

static void *
yc_realloc(void *old, size_t size)
{
    void *p;
    int empty;

    empty = old == NULL;
    p = realloc(old, size);
    yc.live += empty && p != NULL;
    return p;
}

static void
yc_free(void *p)
{
    yc.live -= p != NULL;
    free(p);
}

static sixel_cms_profile_t *
yc_open(void const *bytes, size_t size)
{
    if (yc.armed && yc.fault == 3) {
        ++yc.injected;
        return NULL;
    }
    return sixel_cms_open_profile_from_mem(bytes, size);
}

static int
yc_rgba(TIFF *tif, uint32_t w, uint32_t h, uint32_t *raster,
        int orientation, int stop)
{
    unsigned char source[18];
    uint16_t photo, bits, samples, subx, suby;
    size_t i;
    int ok;

    ++yc.reads;
    ok = TIFFReadRGBAImageOriented(tif, w, h, raster, orientation, stop);
    if (!ok) { return 0; }
    TIFFGetFieldDefaulted(tif, TIFFTAG_PHOTOMETRIC, &photo);
    TIFFGetFieldDefaulted(tif, TIFFTAG_BITSPERSAMPLE, &bits);
    TIFFGetFieldDefaulted(tif, TIFFTAG_SAMPLESPERPIXEL, &samples);
    TIFFGetFieldDefaulted(tif, TIFFTAG_YCBCRSUBSAMPLING, &subx, &suby);
    yc.ok &= w == 3u && h == 2u && photo == PHOTOMETRIC_YCBCR &&
        bits == 8u && samples == 3u && subx == 1u && suby == 1u;
    yc.ok &= TIFFReadEncodedStrip(tif, 0u, source, sizeof(source)) == 18;
    for (i = 0u; i < 18u; ++i) {
        yc.ok &= source[i] == tl_cases[3].source[i];
    }
    for (i = 0u; i < 6u; ++i) {
        yc.ok &= TIFFGetR(raster[i]) == yc_decoded[i * 3u] &&
            TIFFGetG(raster[i]) == yc_decoded[i * 3u + 1u] &&
            TIFFGetB(raster[i]) == yc_decoded[i * 3u + 2u] &&
            TIFFGetA(raster[i]) == 255u;
    }
    return ok;
}

static int
yc_linear(unsigned char *pixels, int w, int h, int format,
          sixel_cms_profile_t *profile)
{
    int ok;

    ++yc.applies;
    yc.ok &= w == 3 && h == 2 && format == SIXEL_PIXELFORMAT_RGBFLOAT32;
    yc.ok &= memcmp(pixels, yc_source, sizeof(yc_source)) == 0;
    ok = sixel_cms_convert_profile_to_linearrgb(pixels,
        yc.fault == 1 ? 2 : w, yc.fault == 1 ? 1 : h, format, profile);
    yc.ok &= ok != 0;
    yc.real_pixels += ok ? (yc.fault == 1 ? 2 : 6) : 0;
    if (ok && yc.fault == 1) {
        /* The real CMM writes a changed prefix into private float storage.
         * Returning false then probes this adapter's whole-RGBA fallback.
         * Shared-helper internal rollback has its own existing fault tests. */
        yc.ok &= memcmp(pixels, yc.native, 6u * sizeof(float)) == 0 &&
            memcmp(pixels, yc_source, 6u * sizeof(float)) != 0;
        ++yc.injected;
        return 0;
    }
    return ok;
}

static int
yc_target_format(void)
{
    return yc.target == 1 ? SIXEL_PIXELFORMAT_RGBFLOAT32
         : yc.target == 2 ? SIXEL_PIXELFORMAT_RGB888
         : yc.target == 3 ? SIXEL_PIXELFORMAT_OKLABFLOAT32
         : SIXEL_PIXELFORMAT_LINEARRGBFLOAT32;
}

#define g_sixel_loader_libtiff_vtbl yc_local_0
#define load_tiff yc_local_1
#define load_with_libtiff yc_local_2
#define loader_can_try_libtiff yc_local_3
#define sixel_loader_libtiff_component yc_local_4
#define sixel_loader_libtiff_component_t yc_local_5
#define sixel_loader_libtiff_load yc_local_6
#define sixel_loader_libtiff_name yc_local_7
#define sixel_loader_libtiff_new yc_local_8
#define sixel_loader_libtiff_placeholder yc_local_9
#define sixel_loader_libtiff_placeholder_function yc_local_10
#define sixel_loader_libtiff_predicate yc_local_11
#define sixel_loader_libtiff_ref yc_local_12
#define sixel_loader_libtiff_setopt yc_local_13
#define sixel_loader_libtiff_unref yc_local_14
#define tiff_clamp_lab_ab yc_local_15
#define tiff_clamp_unit yc_local_16
#define tiff_convert_embedded_icc_to_srgb yc_local_17
#define tiff_convert_embedded_icc_to_srgb_nolcms yc_local_19
#define tiff_decode_lab_u16_ab yc_local_21
#define tiff_decode_lab_u16_l yc_local_22
#define tiff_map_scanline_coordinates yc_local_23
#define tiff_memory_chunk yc_local_24
#define tiff_memory_chunk_t yc_local_25
#define tiff_memory_close yc_local_26
#define tiff_memory_map yc_local_27
#define tiff_memory_read yc_local_28
#define tiff_memory_seek yc_local_29
#define tiff_memory_size yc_local_30
#define tiff_memory_unmap yc_local_31
#define tiff_memory_write yc_local_32
#define tiff_normalize_lab_ab_sample yc_local_33
#define tiff_normalize_lab_l_sample yc_local_34
#define tiff_orientation_is_supported yc_local_35
#define tiff_photometric_allows_embedded_icc yc_local_36
#define tiff_try_load_high_precision yc_local_37
#define tiff_logluv_xyz_to_linear yc_local_38
#define tiff_convert_samples_to_linear yc_local_39
#define tiff_try_load_integer_alpha yc_local_40
#define tiff_read_separate_uint16 yc_separate
#define TIFFReadRGBAImageOriented yc_rgba
#define sixel_cms_convert_profile_to_linearrgb yc_linear
#define loader_cms_target_pixelformat yc_target_format
#define tiff_publish_integer_alpha yc_ns_0
#define tiff_integer_layout_t yc_ns_1
#define tiff_scatter_integer_row yc_ns_2
#define tiff_try_load_integer_layouts yc_ns_3
#define sixel_cms_open_profile_from_mem yc_open
#define tiff_lab8_ranges tlx_copy_9_ranges
#define tiff_lab8_to_linear tlx_copy_9_linear
#include "src/loader-libtiff.c"
#undef tiff_lab8_to_linear
#undef tiff_lab8_ranges
#undef sixel_cms_open_profile_from_mem
#undef tiff_publish_integer_alpha
#undef tiff_integer_layout_t
#undef tiff_scatter_integer_row
#undef tiff_try_load_integer_layouts

#undef g_sixel_loader_libtiff_vtbl
#undef load_tiff
#undef load_with_libtiff
#undef loader_can_try_libtiff
#undef sixel_loader_libtiff_component
#undef sixel_loader_libtiff_component_t
#undef sixel_loader_libtiff_load
#undef sixel_loader_libtiff_name
#undef sixel_loader_libtiff_new
#undef sixel_loader_libtiff_placeholder
#undef sixel_loader_libtiff_placeholder_function
#undef sixel_loader_libtiff_predicate
#undef sixel_loader_libtiff_ref
#undef sixel_loader_libtiff_setopt
#undef sixel_loader_libtiff_unref
#undef tiff_clamp_lab_ab
#undef tiff_clamp_unit
#undef tiff_convert_embedded_icc_to_srgb
#undef tiff_convert_embedded_icc_to_srgb_nolcms
#undef tiff_decode_lab_u16_ab
#undef tiff_decode_lab_u16_l
#undef tiff_map_scanline_coordinates
#undef tiff_memory_chunk
#undef tiff_memory_chunk_t
#undef tiff_memory_close
#undef tiff_memory_map
#undef tiff_memory_read
#undef tiff_memory_seek
#undef tiff_memory_size
#undef tiff_memory_unmap
#undef tiff_memory_write
#undef tiff_normalize_lab_ab_sample
#undef tiff_normalize_lab_l_sample
#undef tiff_orientation_is_supported
#undef tiff_photometric_allows_embedded_icc
#undef tiff_try_load_high_precision
#undef tiff_logluv_xyz_to_linear
#undef tiff_convert_samples_to_linear
#undef tiff_try_load_integer_alpha
#undef tiff_read_separate_uint16
#undef TIFFReadRGBAImageOriented
#undef sixel_cms_convert_profile_to_linearrgb
#undef loader_cms_target_pixelformat

static SIXELSTATUS
yc_capture(sixel_frame_t *frame, void *context)
{
    size_t i;
    int format, space, value;

    (void)context;
    ++yc.calls;
    format = yc.managed ? yc_target_format() : SIXEL_PIXELFORMAT_RGB888;
    space = !yc.managed || yc.target == 1 || yc.target == 2
        ? SIXEL_COLORSPACE_GAMMA : yc.target == 3
        ? SIXEL_COLORSPACE_OKLAB : SIXEL_COLORSPACE_LINEAR;
    yc.ok &= frame->width == 3 && frame->height == 2 &&
        frame->pixelformat == format && frame->colorspace == space &&
        frame->transparent_mask_size == 0u &&
        frame->alpha_zero_is_transparent == 0;
    if (!yc.ok) { return SIXEL_BAD_INPUT; }
    if (format == SIXEL_PIXELFORMAT_RGB888) {
        for (i = 0u; i < 18u; ++i) {
            value = yc.managed ? (int)(fmax(0.0, fmin(1.0,
                yc.expected[i])) * 255.0 + 0.5) : yc_decoded[i];
            yc.ok &= frame->pixels.u8ptr[i] == value;
        }
    } else {
        yc.ok &= memcmp(frame->pixels.u8ptr, yc.expected,
                        sizeof(yc.expected)) == 0;
    }
    return yc.ok ? SIXEL_OK : SIXEL_BAD_INPUT;
}

static int
yc_reference(char const *path)
{
    TIFF *tif;
    void *bytes;
    uint32_t length;
    sixel_cms_profile_t *profile;
    double scale;
    size_t i, j;
    int ok;

    tif = TIFFOpen(path, "r");
    if (tif == NULL) { return 0; }
    profile = NULL;
    bytes = NULL;
    length = 0u;
    ok = TIFFGetField(tif, TIFFTAG_ICCPROFILE, &length, &bytes);
    if (ok) { profile = sixel_cms_open_profile_from_mem(bytes, length); }
    memcpy(yc.native, yc_source, sizeof(yc.native));
    ok = profile != NULL && sixel_cms_convert_profile_to_linearrgb(
        (unsigned char *)yc.native, 3, 2, SIXEL_PIXELFORMAT_RGBFLOAT32,
        profile);
    if (profile != NULL) { sixel_cms_close_profile(profile); }
    TIFFClose(tif);
    /* Native controls establish adapter equivalence with the same selected
     * engine. Builtin additionally has independent stored-ICC math bounds. */
    for (i = 0u; ok && yc.engine == 2 && i < 18u; i += 3u) {
        scale = 0.0;
        for (j = 0u; j < 3u; ++j) {
            scale = fmax(scale, fabs(yc_linear_values[i + j]));
        }
        for (j = 0u; j < 3u; ++j) {
            ok &= fabs(yc.native[i + j] - yc_linear_values[i + j]) <=
                scale * 64.0 / 16777216.0;
        }
    }
    return ok;
}

static int
yc_phase(sixel_loader_component_t *component, sixel_chunk_t *chunk,
         int enabled, int fault, int target)
{
    SIXELSTATUS status;
    int live, applies;

    yc.managed = enabled && !yc.mismatch && fault == 0;
    yc.fault = fault;
    yc.target = target;
    yc.calls = yc.reads = yc.applies = yc.injected = yc.real_pixels = 0;
    memcpy(yc.expected, yc.native, sizeof(yc.expected));
    if (target != 0 && yc.managed) {
        status = sixel_color_convert_rgbf32(yc.expected, 6u,
            SIXEL_COLORSPACE_LINEAR, target == 3
                ? SIXEL_COLORSPACE_OKLAB : SIXEL_COLORSPACE_GAMMA);
        if (SIXEL_FAILED(status)) { return 0; }
    }
    applies = enabled && !yc.mismatch && fault != 2 && fault != 3;
    live = yc.live;
    status = component->vtbl->setopt(component,
        SIXEL_LOADER_COMPONENT_OPTION_LIBTIFF_ENABLE_CMS, &enabled);
    if (SIXEL_FAILED(status)) { return 0; }
    fprintf(stderr, "TYC|begin|engine=%s|src=4|applies=%d\n",
        sixel_cms_engine_to_string((sixel_cms_engine_t)yc.engine), applies);
    yc.armed = 1;
    status = component->vtbl->load(component, chunk, yc_capture, NULL);
    yc.armed = 0;
    yc.ok &= status == SIXEL_OK && yc.calls == 1 && yc.reads == 1 &&
        yc.applies == applies && yc.live == live &&
        yc.injected == (fault != 0) &&
        yc.real_pixels == (applies ? (fault == 1 ? 2 : 6) : 0);
    fprintf(stderr, "TYC|fault=%d|format=%x|calls=%d|cms=%d|real=%d|"
        "injected=%d|ok=%d\n", fault, yc_target_format(), yc.calls,
        yc.applies, yc.real_pixels, yc.injected, yc.ok);
    fprintf(stderr, "TYC|end|ok=%d\n", yc.ok);
    ++yc.phases;
    return yc.ok;
}
#endif

int
tiff_ycbcr_test(int argc, char **argv, int engine, int mode)
{
#if HAVE_LIBTIFF
    sixel_allocator_t *allocator;
    sixel_loader_component_t *component;
    sixel_chunk_t *chunk, *mismatch, *optional;
    sixel_cms_engine_t saved;
    void *object;
    unsigned char original[4096], changed[4096];
    size_t bytes, entry, profile_offset;
    unsigned int entries, tag;
    char path[4096];
    int ok, target, variant;

    if (argc != 2) { return 2; }
    memset(&yc, 0, sizeof(yc));
    yc.engine = engine;
    yc.ok = 1;
    saved = sixel_cms_get_engine();
    sixel_cms_set_engine((sixel_cms_engine_t)engine);
    if ((int)sixel_cms_get_engine() != engine) {
        sixel_cms_set_engine(saved);
        return 77;
    }
    allocator = NULL;
    component = NULL;
    chunk = mismatch = optional = NULL;
    object = NULL;
    ok = 0;
    snprintf(path, sizeof(path), "%s/tiff-legacy-ycbcr8-111.tif", argv[1]);
    if (!yc_reference(path)) { goto cleanup; }
    if (SIXEL_FAILED(sixel_allocator_new(&allocator, yc_malloc,
        yc_calloc, yc_realloc, yc_free))) { goto cleanup; }
    if (SIXEL_FAILED(sixel_chunk_create_from_source(&chunk, path, 0,
        NULL, allocator))) { goto cleanup; }
    bytes = sixel_chunk_get_size(chunk);
    if (bytes > sizeof(original)) { goto cleanup; }
    memcpy(original, sixel_chunk_get_buffer(chunk), bytes);
    if (SIXEL_FAILED(yc_local_8(allocator, &object))) { goto cleanup; }
    component = object;
    if (mode == 4) {
        if (!yc_phase(component, chunk, 0, 0, 0)) { goto cleanup; }
        snprintf(path, sizeof(path),
            "%s/tiff-legacy-ycbcr8-111-mismatch.tif", argv[1]);
        if (SIXEL_FAILED(sixel_chunk_create_from_source(&mismatch, path, 0,
            NULL, allocator))) { goto cleanup; }
        yc.mismatch = 1;
        if (!yc_phase(component, mismatch, 1, 0, 0)) { goto cleanup; }
        /* Derive absent and invalid ICC inputs in memory from the same
         * literal fixture. Neither changes any original sample or creates
         * another file. The original borrowed input remains untouched. */
        if (bytes < 14u || original[0] != 'I' || original[1] != 'I' ||
            original[4] != 8u) { goto cleanup; }
        entries = original[8] | ((unsigned int)original[9] << 8);
        profile_offset = 0u;
        for (entry = 10u; entry + 12u <= bytes &&
                         entry < 10u + entries * 12u; entry += 12u) {
            tag = original[entry] | ((unsigned int)original[entry + 1] << 8);
            if (tag == 34675u) {
                profile_offset = original[entry + 8] |
                    ((size_t)original[entry + 9] << 8) |
                    ((size_t)original[entry + 10] << 16) |
                    ((size_t)original[entry + 11] << 24);
                break;
            }
        }
        if (profile_offset == 0u || profile_offset + 40u > bytes) {
            goto cleanup;
        }
        for (variant = 0; variant < 2; ++variant) {
            memcpy(changed, original, bytes);
            if (variant == 0) {
                /* Unknown private tag: ICCPROFILE is absent to libtiff. */
                changed[entry] = 0xe8u;
                changed[entry + 1u] = 0xfdu;
            } else {
                /* An inline four-byte value is a truncated ICC header. */
                changed[entry + 4u] = 4u;
                memset(changed + entry + 5u, 0, 3u);
                memcpy(changed + entry + 8u, "bad!", 4u);
            }
            if (SIXEL_FAILED(sixel_chunk_create_from_memory(&optional,
                changed, bytes, NULL, allocator))) { goto cleanup; }
            if (!yc_phase(component, optional, 1, 0, 0) ||
                memcmp(changed, sixel_chunk_get_buffer(optional), bytes)) {
                goto cleanup;
            }
            optional->vtbl->unref(optional);
            optional = NULL;
        }
        yc.mismatch = 0;
        if (!yc_phase(component, chunk, 1, 0, 0)) { goto cleanup; }
    } else if (mode == 5) {
        for (target = 1; target <= 3; ++target) {
            if (!yc_phase(component, chunk, 1, 0, target)) {
                goto cleanup;
            }
        }
    } else {
        if (!yc_phase(component, chunk, 1, 0, 0)) { goto cleanup; }
        if (mode != 0 &&
            (!yc_phase(component, chunk, 1, mode, 0) ||
             !yc_phase(component, chunk, 1, 0, 0))) { goto cleanup; }
    }
    ok = memcmp(original, sixel_chunk_get_buffer(chunk), bytes) == 0;
cleanup:
    if (component != NULL) { component->vtbl->unref(component); }
    if (chunk != NULL) { chunk->vtbl->unref(chunk); }
    if (mismatch != NULL) { mismatch->vtbl->unref(mismatch); }
    if (optional != NULL) { optional->vtbl->unref(optional); }
    sixel_allocator_unref(allocator);
    sixel_cms_set_engine(saved);
    return ok && yc.ok && yc.live == 0 ? 0 : 1;
#else
    (void)argc; (void)argv; (void)engine; (void)mode;
    return 77;
#endif
}
