/* SPDX-License-Identifier: MIT */
/* Check the real decoder's packed CMS input and whole-source rollback after
 * a qualified shared-CMM apply has overwritten its destination prefix. */
#include "config.h"
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "src/cms.h"
#include "src/allocator.h"
#include "src/loader-libjpeg.h"
#include "tests/loader/chunk_test_common.h"

#if HAVE_JPEG
#include "tests/data/color-numeric/jpeg-precision.h"

static int jf_fixture;
static int jf_inject;
static int jf_calls;
static int jf_bad_source;
static sixel_cms_pixel_format_t jf_format;

static sixel_cms_transform_t *
jf_create(sixel_cms_profile_t const *src, sixel_cms_pixel_format_t src_format,
             sixel_cms_profile_t const *dst,
             sixel_cms_pixel_format_t dst_format,
             int flags)
{
    jf_format = src_format;
    return sixel_cms_create_transform(src, src_format, dst, dst_format, flags);
}

static int
jf_apply(sixel_cms_transform_t const *transform, void const *input,
            void *output, size_t count)
{
    unsigned char const *bytes;
    uint16_t const *words;
    unsigned int sample;
    unsigned int actual;
    unsigned int maximum;
    size_t i;
    size_t c;
    size_t offset;
    int result;

    ++jf_calls;
    bytes = input;
    words = input;
    maximum = jf_fixture == 0 ? 255u : 65535u;
    if (count != 4096u || jf_format != (jf_fixture == 0 ?
        SIXEL_CMS_PIXELFORMAT_CMYK_8 : SIXEL_CMS_PIXELFORMAT_CMYK_16)) {
        jf_bad_source = 1;
        return 0;
    }
    for (i = 0u; i < 32u; ++i) {
        for (c = 0u; c < 4u; ++c) {
            sample = jp_raw[jf_fixture][i][c];
            if (jf_fixture == 1 || jf_fixture == 2) {
                sample = (sample * 65535u + 2047u) / 4095u;
            }
            offset = (size_t)jp_witness[i] * 4u + c;
            actual = jf_fixture == 0 ? bytes[offset] : words[offset];
            if (actual != maximum - sample) { jf_bad_source = 1; }
        }
    }
    result = sixel_cms_do_transform(transform, input, output, count);
    if (result && jf_inject) {
        ((float *)output)[0] = 0.5f;
        return 0;
    }
    return result;
}

#define g_sixel_loader_libjpeg_vtbl jf_local_0
#define jpeg_collect_icc_profile jf_local_1
#define jpeg_convert_icc_cmyk16_to_srgb_f32 jf_local_2
#define jpeg_convert_icc_cmyk8_to_srgb_f32 jf_local_3
#define jpeg_normalize_rgbf32 jf_local_4
#define jpeg_convert_rgbf32_gamma_to_linear jf_local_5
#define jpeg_decode_cmyk16_from_precision jf_local_6
#define jpeg_decode_to_float32_from_precision jf_local_7
#define jpeg_parse_exif_orientation jf_local_8
#define jpeg_promote_cmyk16_to_linear_float32 jf_local_9
#define jpeg_promote_cmyk888_to_linear_float32 jf_local_10
#define jpeg_promote_rgb888_to_linear_float32 jf_local_11
#define jpeg_read_u16be jf_local_12
#define jpeg_unpack_cmyk16_to_rgbf32 jf_local_13
#define jpeg_unpack_cmyk8_to_rgbf32 jf_local_14
#define jpeg_unpack_rgb12_to_rgbf32 jf_local_15
#define jpeg_unpack_rgb16_to_rgbf32 jf_local_16
#define jpeg_unpack_rgb8_to_rgbf32 jf_local_17
#define load_jpeg jf_load_jpeg
#define load_with_libjpeg jf_local_19
#define loader_can_try_libjpeg jf_local_20
#define sixel_loader_libjpeg_component jf_local_21
#define sixel_loader_libjpeg_component_t jf_local_22
#define sixel_loader_libjpeg_error_context jf_local_23
#define sixel_loader_libjpeg_error_context_t jf_local_24
#define sixel_loader_libjpeg_error_exit jf_local_25
#define sixel_loader_libjpeg_load jf_local_26
#define sixel_loader_libjpeg_name jf_local_27
#define sixel_loader_libjpeg_new jf_local_28
#define sixel_loader_libjpeg_placeholder jf_local_29
#define sixel_loader_libjpeg_placeholder_function jf_local_30
#define sixel_loader_libjpeg_predicate jf_local_31
#define sixel_loader_libjpeg_ref jf_local_32
#define sixel_loader_libjpeg_setopt jf_local_33
#define sixel_loader_libjpeg_unref jf_local_34
#define sixel_cms_create_transform jf_create
#define sixel_cms_do_transform jf_apply

#include "src/loader-libjpeg.c"

#undef g_sixel_loader_libjpeg_vtbl
#undef jpeg_collect_icc_profile
#undef jpeg_convert_icc_cmyk16_to_srgb_f32
#undef jpeg_convert_icc_cmyk8_to_srgb_f32
#undef jpeg_normalize_rgbf32
#undef jpeg_convert_rgbf32_gamma_to_linear
#undef jpeg_decode_cmyk16_from_precision
#undef jpeg_decode_to_float32_from_precision
#undef jpeg_parse_exif_orientation
#undef jpeg_promote_cmyk16_to_linear_float32
#undef jpeg_promote_cmyk888_to_linear_float32
#undef jpeg_promote_rgb888_to_linear_float32
#undef jpeg_read_u16be
#undef jpeg_unpack_cmyk16_to_rgbf32
#undef jpeg_unpack_cmyk8_to_rgbf32
#undef jpeg_unpack_rgb12_to_rgbf32
#undef jpeg_unpack_rgb16_to_rgbf32
#undef jpeg_unpack_rgb8_to_rgbf32
#undef load_jpeg
#undef load_with_libjpeg
#undef loader_can_try_libjpeg
#undef sixel_loader_libjpeg_component
#undef sixel_loader_libjpeg_component_t
#undef sixel_loader_libjpeg_error_context
#undef sixel_loader_libjpeg_error_context_t
#undef sixel_loader_libjpeg_error_exit
#undef sixel_loader_libjpeg_load
#undef sixel_loader_libjpeg_name
#undef sixel_loader_libjpeg_new
#undef sixel_loader_libjpeg_placeholder
#undef sixel_loader_libjpeg_placeholder_function
#undef sixel_loader_libjpeg_predicate
#undef sixel_loader_libjpeg_ref
#undef sixel_loader_libjpeg_setopt
#undef sixel_loader_libjpeg_unref
#undef sixel_cms_create_transform
#undef sixel_cms_do_transform

static int
jf_values(unsigned char const *pixels, int fallback)
{
    float const *rgb;
    double expected;
    double actual;
    size_t i;
    size_t c;

    rgb = (float const *)pixels;
    for (i = 0u; i < 4096u * 3u; ++i) {
        if (!isfinite(rgb[i])) { return 0; }
    }
    for (i = 0u; i < 32u; ++i) {
        for (c = 0u; c < 3u; ++c) {
            expected = fallback ? jp_fallback[jf_fixture][1][i][c]
                                : jp_linear[jf_fixture][i][c];
            actual = rgb[(size_t)jp_witness[i] * 3u + c];
            if (fabs(actual - expected) >
                fabs(expected) * (64.0 / 16777216.0)) {
                return 0;
            }
        }
    }
    return 1;
}
#endif

int
test_jp_fault(int argc, char **argv)
{
#if HAVE_JPEG
    static char const *const names[] = {
        "8bit-ycck-seq444", "12bit-cmyk-seq444",
        "12bit-ycck-seq444", "16bit-cmyk-lossless"
    };
    sixel_allocator_t *allocator;
    sixel_chunk_t *chunk;
    sixel_cms_engine_t previous;
    unsigned char *pixels;
    char path[4096];
    SIXELSTATUS status;
    int width;
    int height;
    int format;
    int length;
    int stage;
    int ok;

    if (argc != 3) { return 2; }
    jf_fixture = atoi(argv[2]);
    if (jf_fixture < 0 || jf_fixture > 3) { return 2; }
#if !HAVE_JPEG12_API
    if (jf_fixture == 1 || jf_fixture == 2) { return 77; }
#endif
#if !HAVE_JPEG16_API
    if (jf_fixture == 3) { return 77; }
#endif
    length = snprintf(path, sizeof(path),
        "%s/tests/data/color-numeric/jpeg-precision-%s-affine.jpg",
        argv[1], names[jf_fixture]);
    if (length < 0 || (size_t)length >= sizeof(path)) { return 2; }
    allocator = NULL;
    chunk = NULL;
    pixels = NULL;
    ok = 0;
    jf_calls = jf_bad_source = 0;
    previous = sixel_cms_get_engine();
    sixel_cms_set_engine(SIXEL_CMS_ENGINE_BUILTIN);
    status = sixel_allocator_new(&allocator, NULL, NULL, NULL, NULL);
    if (SIXEL_FAILED(status)) { goto cleanup; }
    status = sixel_chunk_create_from_source(&chunk, path, 0, NULL, allocator);
    if (SIXEL_FAILED(status)) { goto cleanup; }
    for (stage = 0; stage < 2; ++stage) {
        jf_inject = stage;
        status = jf_load_jpeg(&pixels, sixel_chunk_get_buffer(chunk),
            sixel_chunk_get_size(chunk), &width, &height, &format,
            allocator, 1, 1);
        if (SIXEL_FAILED(status) || width != 64 || height != 64 ||
            format != SIXEL_PIXELFORMAT_LINEARRGBFLOAT32 ||
            !jf_values(pixels, stage)) {
            goto cleanup;
        }
        sixel_allocator_free(allocator, pixels);
        pixels = NULL;
    }
    ok = jf_calls == 2 && !jf_bad_source;
cleanup:
    sixel_allocator_free(allocator, pixels);
    if (chunk != NULL) { chunk->vtbl->unref(chunk); }
    sixel_allocator_unref(allocator);
    sixel_cms_set_engine(previous);
    if (!ok) {
        fprintf(stderr, "JPEG prefix fixture=%d status=%x calls=%d source=%d\n",
            jf_fixture, status, jf_calls, !jf_bad_source);
    }
    return ok ? 0 : 1;
#else
    (void)argc;
    (void)argv;
    return 77;
#endif
}

int
test_jp_gray16(int argc, char **argv)
{
#if HAVE_JPEG && HAVE_JPEG16_API
    sixel_allocator_t *allocator;
    sixel_chunk_t *chunk;
    unsigned char *pixels;
    float const *rgb;
    float expected;
    char path[4096];
    SIXELSTATUS status;
    size_t i;
    size_t c;
    int width;
    int height;
    int format;
    int length;
    int ok;

    if (argc != 2) { return 2; }
    length = snprintf(path, sizeof(path),
        "%s/tests/data/inputs/formats/"
        "snake-jpeg-16bit-lossless-gray-restart.jpg", argv[1]);
    if (length < 0 || (size_t)length >= sizeof(path)) { return 2; }
    allocator = NULL;
    chunk = NULL;
    pixels = NULL;
    ok = 0;
    status = sixel_allocator_new(&allocator, NULL, NULL, NULL, NULL);
    if (SIXEL_FAILED(status)) { goto cleanup; }
    status = sixel_chunk_create_from_source(&chunk, path, 0, NULL, allocator);
    if (SIXEL_FAILED(status)) { goto cleanup; }
    status = jf_load_jpeg(&pixels, sixel_chunk_get_buffer(chunk),
        sixel_chunk_get_size(chunk), &width, &height, &format, allocator, 0, 1);
    if (SIXEL_FAILED(status) || width != 64 || height != 64 ||
        format != SIXEL_PIXELFORMAT_RGBFLOAT32) {
        goto cleanup;
    }
    rgb = (float const *)pixels;
    for (i = 0u; i < 4096u * 3u; ++i) {
        if (!isfinite(rgb[i])) { goto cleanup; }
    }
    for (i = 0u; i < 32u; ++i) {
        /* Original decoder words, independently frozen before any CMS;
         * each RGB component must equal the same normalized Gray16 value. */
        expected = (float)((double)jp_gray16_raw[i] / 65535.0);
        for (c = 0u; c < 3u; ++c) {
            if (rgb[(size_t)jp_witness[i] * 3u + c] != expected) {
                goto cleanup;
            }
        }
    }
    ok = 1;
cleanup:
    sixel_allocator_free(allocator, pixels);
    if (chunk != NULL) { chunk->vtbl->unref(chunk); }
    sixel_allocator_unref(allocator);
    return ok ? 0 : 1;
#else
    (void)argc;
    (void)argv;
    return 77;
#endif
}
