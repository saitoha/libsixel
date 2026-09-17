/* SPDX-License-Identifier: MIT */
/* Inject failures at the PNG source-selection and paired-apply boundaries.
 * The included production source is namespaced, not copied. A tiny decoder
 * double supplies the independently recorded integers; the public loader
 * tests separately cover actual PNG decoding and callback delivery. */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "src/cms.h"
#include "tests/loader/chunk_test_common.h"
#include "src/frompng.h"
#include "src/frame-private.h"
#include "src/loader-common.h"
#include "tests/data/color-numeric/png-metadata.h"

#if HAVE_LCMS2
static color_png_metadata_t const *pmfi_source;
static int pmfi_mode;
static int pmfi_factory_calls;
static int pmfi_apply_calls;
static int pmfi_profile_live;
static int pmfi_live;
static int pmfi_bad_free;

static void *
pmfi_malloc(size_t size)
{
    void *p;
    p = malloc(size);
    if (p != NULL) { ++pmfi_live; }
    return p;
}

static void *
pmfi_calloc(size_t count, size_t size)
{
    void *p;
    if (count && size > SIZE_MAX / count) { return NULL; }
    p = pmfi_malloc(count * size);
    if (p != NULL) { memset(p, 0, count * size); }
    return p;
}

static void *
pmfi_realloc(void *p, size_t size)
{
    return p == NULL ? pmfi_malloc(size) : realloc(p, size);
}

static void
pmfi_free(void *p)
{
    if (p != NULL) {
        if (pmfi_live <= 0) { pmfi_bad_free = 1; }
        --pmfi_live;
    }
    free(p);
}

static char const *
pmfi_decoder_reason(void)
{
    return "metadata test decoder";
}

static int
pmfi_is16(unsigned char const *buffer, int length)
{
    (void)buffer;
    (void)length;
    return 0;
}

static unsigned short *
pmfi_load16(unsigned char const *buffer, int length, int *width,
              int *height, int *channels, int requested)
{
    (void)buffer; (void)length; (void)width; (void)height;
    (void)channels; (void)requested;
    return NULL;
}

static unsigned char *
pmfi_load8(unsigned char const *buffer, int length, int *width,
             int *height, int *channels, int requested)
{
    unsigned char *pixels;
    int p;
    int c;

    (void)buffer;
    (void)length;
    *width = 8;
    *height = 1;
    *channels = pmfi_source->type == 2 ? 3 : 4;
    pixels = pmfi_malloc((size_t)requested * 8u);
    if (pixels == NULL) { return NULL; }
    for (p = 0; p < 8; ++p) {
        for (c = 0; c < requested; ++c) {
            pixels[p * requested + c] =
                (unsigned char)pmfi_source->rgba[p * 4 + c];
        }
    }
    return pixels;
}

static char *
pmfi_inflate(char const *buffer, int length, int initial_size,
               int *out_length, int header)
{
    (void)buffer; (void)length; (void)initial_size; (void)header;
    *out_length = 0;
    return NULL;
}

static sixel_cms_profile_t *
pmfi_factory(double gamma, double wx, double wy, double rx, double ry,
               double gx, double gy, double bx, double by)
{
    sixel_cms_profile_t *profile;

    ++pmfi_factory_calls;
    if (pmfi_mode == 1 || pmfi_mode == 2 || pmfi_mode >= 6) {
        return NULL;
    }
    profile = sixel_cms_create_rgb_profile_from_gamma_chrm(gamma,
        wx, wy, rx, ry, gx, gy, bx, by);
    if (profile != NULL) { ++pmfi_profile_live; }
    return profile;
}

static void
pmfi_close(sixel_cms_profile_t *profile)
{
    if (profile != NULL) { --pmfi_profile_live; }
    sixel_cms_close_profile(profile);
}

static int
pmfi_apply(unsigned char *pixels, int width, int height, int format,
             sixel_cms_profile_t *profile)
{
    float original[24];
    size_t size;
    int result;

    size = (size_t)width * height * 3u * sizeof(float);
    if (size > sizeof(original)) { return 0; }
    memcpy(original, pixels, size);
    result = sixel_cms_convert_profile_to_linearrgb(pixels, width, height,
        format, profile);
    ++pmfi_apply_calls;
    if ((pmfi_mode == 3 && pmfi_apply_calls == 1) ||
        (pmfi_mode == 4 && pmfi_apply_calls == 2) || pmfi_mode == 5) {
        /* Leave exactly a prefix modified when reporting failure. Hidden
         * black RGB is also checked, so alpha-zero cannot hide the fault. */
        memcpy(pixels, original, size);
        ((float *)pixels)[0] = 0.5f;
        return 0;
    }
    return result;
}

/* Keep the test namespace isolated in ordinary and amalgamated builds. */
#define sixel_frompng_extract_icc pmfi_extract_icc
#define sixel_frompng_build_chrm_to_srgb_matrix pmfi_chrm_matrix
#define sixel_frompng_read_be32 pmfi_read_be32
#define sixel_frompng_read_be16 pmfi_read_be16
#define sixel_frompng_parse_ihdr pmfi_parse_ihdr
#define sixel_frompng_expand_sample_to_bg pmfi_expand_sample_to_bg
#define sixel_frompng_parse_bkgd pmfi_parse_bkgd
#define sixel_frompng_resolve_background pmfi_resolve_background
#define sixel_frompng_detect_chunk_flags pmfi_detect_chunk_flags
#define sixel_frompng_build_profile_from_chunks pmfi_chunk_profile
#define sixel_frompng_apply_colorspace_fallback_internal pmfi_fallback_internal
#define sixel_frompng_apply_colorspace_fallback pmfi_fallback
#define sixel_frompng_apply_colorspace_fallback_pixelformat pmfi_fallback_format
#define sixel_frompng_parse_transfer_chunks pmfi_parse_transfer_chunks
#define sixel_frompng_apply_gama_to_srgb_u8 pmfi_gamma_byte
#define sixel_frompng_apply_gama_to_srgb_float32 pmfi_gamma_float
#define sixel_frompng_clamp_unit pmfi_clamp_unit
#define sixel_frompng_decode_srgb_unit pmfi_decode_srgb_unit
#define sixel_frompng_encode_srgb_unit pmfi_encode_srgb_unit
#define sixel_frompng_decode_source_unit pmfi_decode_source_unit
#define sixel_frompng_invert_3x3 pmfi_invert_3x3
#define sixel_frompng_apply_linear_matrix_triplet pmfi_matrix_triplet
#define sixel_frompng_select_source pmfi_select_source
#define sixel_frompng_normalize_samples pmfi_normalize_samples
#define sixel_frompng_decode_samples pmfi_decode_samples
#define sixel_frompng_has_transparency pmfi_has_transparency
#define sixel_frompng_load_nonindexed pmfi_load_nonindexed
#define sixel_frompng_source_t pmfi_source_t
#define SIXEL_FROMPNG_TRANSFER_SRGB PMFI_TRANSFER_SRGB
#define SIXEL_FROMPNG_TRANSFER_GAMA PMFI_TRANSFER_GAMA
#define stbi_failure_reason pmfi_decoder_reason
#define stbi_image_free pmfi_free
#define stbi_is_16_bit_from_memory pmfi_is16
#define stbi_load_16_from_memory pmfi_load16
#define stbi_load_from_memory pmfi_load8
#define stbi_zlib_decode_malloc_guesssize_headerflag pmfi_inflate
#define sixel_cms_create_rgb_profile_from_gamma_chrm pmfi_factory
#define sixel_cms_convert_profile_to_linearrgb pmfi_apply
#define sixel_cms_close_profile pmfi_close

#include "src/frompng.c"

#undef sixel_frompng_extract_icc
#undef sixel_frompng_build_chrm_to_srgb_matrix
#undef sixel_frompng_read_be32
#undef sixel_frompng_read_be16
#undef sixel_frompng_parse_ihdr
#undef sixel_frompng_expand_sample_to_bg
#undef sixel_frompng_parse_bkgd
#undef sixel_frompng_resolve_background
#undef sixel_frompng_detect_chunk_flags
#undef sixel_frompng_build_profile_from_chunks
#undef sixel_frompng_apply_colorspace_fallback_internal
#undef sixel_frompng_apply_colorspace_fallback
#undef sixel_frompng_apply_colorspace_fallback_pixelformat
#undef sixel_frompng_parse_transfer_chunks
#undef sixel_frompng_apply_gama_to_srgb_u8
#undef sixel_frompng_apply_gama_to_srgb_float32
#undef sixel_frompng_clamp_unit
#undef sixel_frompng_decode_srgb_unit
#undef sixel_frompng_encode_srgb_unit
#undef sixel_frompng_decode_source_unit
#undef sixel_frompng_invert_3x3
#undef sixel_frompng_apply_linear_matrix_triplet
#undef sixel_frompng_select_source
#undef sixel_frompng_normalize_samples
#undef sixel_frompng_decode_samples
#undef sixel_frompng_has_transparency
#undef sixel_frompng_load_nonindexed
#undef sixel_frompng_source_t
#undef SIXEL_FROMPNG_TRANSFER_SRGB
#undef SIXEL_FROMPNG_TRANSFER_GAMA
#undef stbi_failure_reason
#undef stbi_image_free
#undef stbi_is_16_bit_from_memory
#undef stbi_load_16_from_memory
#undef stbi_load_from_memory
#undef stbi_zlib_decode_malloc_guesssize_headerflag
#undef sixel_cms_create_rgb_profile_from_gamma_chrm
#undef sixel_cms_convert_profile_to_linearrgb
#undef sixel_cms_close_profile

#endif

int
test_loader_0193_png_metadata_failures(int argc, char **argv)
{
#if HAVE_LCMS2
    sixel_chunk_t *chunk;
    sixel_allocator_t *allocator;
    sixel_frame_t *frame;
    sixel_cms_engine_t old_engine;
    SIXELSTATUS status;
    float const *expected;
    float value;
    unsigned char const *pixels;
    int fixture;
    int p;
    int c;
    int result;
    int factory_count;
    int apply_count;

    if (argc != 2) { return 1; }
    pmfi_mode = atoi(argv[1]);
    if (pmfi_mode < 0 || pmfi_mode > 7) { return 1; }
    fixture = pmfi_mode == 1 ? 0 : pmfi_mode == 6 ? 1 :
        pmfi_mode == 7 ? 17 : 16;
    pmfi_source = &color_png_metadata[fixture];
    pmfi_live = pmfi_bad_free = pmfi_profile_live = 0;
    pmfi_factory_calls = pmfi_apply_calls = 0;
    factory_count = pmfi_mode >= 6 ? 0 : 1;
    apply_count = pmfi_mode == 0 || (pmfi_mode >= 3 && pmfi_mode <= 5)
        ? 2 : 0;
    expected = pmfi_mode == 0 ? pmfi_source->linear : pmfi_source->gamma;
    frame = NULL;
    chunk = NULL;
    allocator = NULL;
    result = 1;
    old_engine = sixel_cms_get_engine();
    sixel_cms_set_engine(SIXEL_CMS_ENGINE_BUILTIN);
    status = sixel_allocator_new(&allocator, pmfi_malloc, pmfi_calloc,
        pmfi_realloc, pmfi_free);
    if (SIXEL_FAILED(status)) { goto cleanup; }
    status = sixel_frame_new(&frame, allocator);
    if (SIXEL_FAILED(status)) { goto cleanup; }
    status = sixel_chunk_create_from_memory(&chunk, pmfi_source->bytes,
        pmfi_source->length, NULL, allocator);
    if (SIXEL_FAILED(status)) { goto cleanup; }
    status = pmfi_load_nonindexed(chunk, allocator, frame, 1, NULL, 0);
    if (SIXEL_FAILED(status) || pmfi_factory_calls != factory_count ||
        pmfi_apply_calls != apply_count || pmfi_profile_live != 0) {
        goto cleanup;
    }
    pixels = sixel_frame_get_pixels(frame);
    if (pmfi_mode == 1) {
        if (sixel_frame_get_pixelformat(frame) != SIXEL_PIXELFORMAT_RGB888 ||
            sixel_frame_get_colorspace(frame) != SIXEL_COLORSPACE_GAMMA) {
            goto cleanup;
        }
        for (p = 0; p < 8; ++p) {
            for (c = 0; c < 3; ++c) {
                if (pixels[p * 3 + c] != pmfi_source->rgba[p * 4 + c]) {
                    goto cleanup;
                }
            }
        }
    } else {
        if (sixel_frame_get_pixelformat(frame) !=
                SIXEL_PIXELFORMAT_LINEARRGBFLOAT32 ||
            sixel_frame_get_colorspace(frame) != SIXEL_COLORSPACE_LINEAR) {
            goto cleanup;
        }
        for (p = 0; p < 24; ++p) {
            memcpy(&value, pixels + p * sizeof(float), sizeof(value));
            if (value != expected[p]) {
                fprintf(stderr, "PNG failure mode=%d sample=%d %a/%a\n",
                    pmfi_mode, p, (double)value, (double)expected[p]);
                goto cleanup;
            }
        }
    }
    result = 0;
cleanup:
    if (chunk != NULL) { chunk->vtbl->unref(chunk); }
    sixel_frame_unref(frame);
    sixel_allocator_unref(allocator);
    sixel_cms_set_engine(old_engine);
    if (pmfi_live != 0 || pmfi_bad_free || pmfi_profile_live != 0) {
        result = 1;
    }
    if (result != 0) {
        fprintf(stderr, "PNG metadata failure status=%x factory=%d/%d "
            "apply=%d/%d live=%d profile=%d\n", status, pmfi_factory_calls,
            factory_count, pmfi_apply_calls, apply_count, pmfi_live,
            pmfi_profile_live);
    }
    return result;
#else
    (void)argc;
    (void)argv;
    return 77;
#endif
}
