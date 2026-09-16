/* SPDX-License-Identifier: MIT */
/* Private actual-adapter probe: read qualification is separate from publish. */
#include "config.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "src/allocator.h"
#include "src/chunk-factory.h"
#include "src/frame-private.h"
#include "src/loader-libtiff.h"
#include "tests/data/color-numeric/tiff-float.h"
#include "tiff_float_test_common.h"
#if HAVE_LIBTIFF
#include <tiffio.h>

static int uf_early;
static int uf_injected;
static int uf_width;
static int uf_height;
static int uf_reads;
static int uf_rows;
static int uf_matched;
static int uf_seen;
static int uf_rgba;
static uint32_t uf_bad;
static size_t uf_live;

static void *
uf_malloc(size_t size)
{
    void *p;

    p = malloc(size);
    if (p != NULL) { ++uf_live; }
    return p;
}

static void *
uf_calloc(size_t n, size_t size)
{
    void *p;

    p = calloc(n, size);
    if (p != NULL) { ++uf_live; }
    return p;
}

static void *
uf_realloc(void *ptr, size_t size)
{
    void *p;
    int empty;

    empty = ptr == NULL;
    p = realloc(ptr, size);
    if (p != NULL && empty) { ++uf_live; }
    return p;
}

static void
uf_free(void *ptr)
{
    if (ptr != NULL) { --uf_live; }
    free(ptr);
}

static int
uf_read(TIFF *tif, void *data, uint32_t row, uint16_t sample)
{
    uint32_t width;
    uint16_t samples;
    uint32_t word;
    uint32_t expected;
    size_t first;
    size_t i;
    int status;

    ++uf_reads;
    if ((uf_early == 1 && row == 0u) ||
        (uf_early == 2 && row == 1u && uf_rows == 1)) {
        ++uf_injected;
        return -1;
    }
    status = TIFFReadScanline(tif, data, row, sample);
    if (status != 1) { return status; }
    ++uf_rows;
    TIFFGetField(tif, TIFFTAG_IMAGEWIDTH, &width);
    TIFFGetFieldDefaulted(tif, TIFFTAG_SAMPLESPERPIXEL, &samples);
    first = (size_t)row * width * samples;
    for (i = 0u; i < (size_t)width * samples; ++i) {
        memcpy(&word, (unsigned char *)data + i * 4u, sizeof(word));
        expected = first + i == 23u && uf_bad ? uf_bad
                 : first + i < 24u ? tuf_words[first + i] : 0u;
        uf_matched += word == expected;
        uf_seen += first + i == 23u && uf_bad && word == uf_bad;
    }
    return status;
}

static int
uf_rgba_read(TIFF *tif, uint32_t width, uint32_t height, uint32_t *raster,
             int orientation, int stop)
{
    ++uf_rgba;
    return TIFFReadRGBAImageOriented(tif, width, height, raster,
                                     orientation, stop);
}

#define g_sixel_loader_libtiff_vtbl uff_local_0
#define load_tiff uff_local_1
#define load_with_libtiff uff_local_2
#define loader_can_try_libtiff uff_local_3
#define sixel_loader_libtiff_component uff_local_4
#define sixel_loader_libtiff_component_t uff_local_5
#define sixel_loader_libtiff_load uff_local_6
#define sixel_loader_libtiff_name uff_local_7
#define sixel_loader_libtiff_new uf_new
#define sixel_loader_libtiff_placeholder uff_local_9
#define sixel_loader_libtiff_placeholder_function uff_local_10
#define sixel_loader_libtiff_predicate uff_local_11
#define sixel_loader_libtiff_ref uff_local_12
#define sixel_loader_libtiff_setopt uff_local_13
#define sixel_loader_libtiff_unref uff_local_14
#define tiff_clamp_lab_ab uff_local_15
#define tiff_clamp_unit uff_local_16
#define tiff_convert_embedded_icc_to_srgb uff_local_17
#define tiff_convert_embedded_icc_to_srgb_float32 uff_local_18
#define tiff_convert_embedded_icc_to_srgb_nolcms uff_local_19
#define tiff_convert_rgbf32_gamma_to_linear uff_local_20
#define tiff_decode_lab_u16_ab uff_local_21
#define tiff_decode_lab_u16_l uff_local_22
#define tiff_logluv_xyz_to_linear uff_local_23
#define tiff_map_scanline_coordinates uff_local_24
#define tiff_memory_chunk uff_local_25
#define tiff_memory_chunk_t uff_local_26
#define tiff_memory_close uff_local_27
#define tiff_memory_map uff_local_28
#define tiff_memory_read uff_local_29
#define tiff_memory_seek uff_local_30
#define tiff_memory_size uff_local_31
#define tiff_memory_unmap uff_local_32
#define tiff_memory_write uff_local_33
#define tiff_normalize_lab_ab_sample uff_local_34
#define tiff_normalize_lab_l_sample uff_local_35
#define tiff_orientation_is_supported uff_local_36
#define tiff_photometric_allows_embedded_icc uff_local_37
#define tiff_try_load_high_precision uff_local_38
#define TIFFReadScanline uf_read
#define TIFFReadRGBAImageOriented uf_rgba_read
#define tiff_convert_samples_to_linear uff_byte
#define tiff_try_load_integer_alpha uff_alpha
#include "src/loader-libtiff.c"
#undef tiff_try_load_integer_alpha
#undef tiff_convert_samples_to_linear
#undef TIFFReadRGBAImageOriented
#undef TIFFReadScanline
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
#undef tiff_convert_embedded_icc_to_srgb_float32
#undef tiff_convert_embedded_icc_to_srgb_nolcms
#undef tiff_convert_rgbf32_gamma_to_linear
#undef tiff_decode_lab_u16_ab
#undef tiff_decode_lab_u16_l
#undef tiff_logluv_xyz_to_linear
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

typedef struct uf_capture {
    int calls;
    int count;
    uint32_t words[72];
} uf_capture_t;

static SIXELSTATUS
uf_capture(sixel_frame_t *frame, void *context)
{
    uf_capture_t *state;

    state = context;
    ++state->calls;
    state->count = frame->width * frame->height * 3;
    if (frame->width != uf_width || frame->height != uf_height ||
        state->count > 72 ||
        frame->pixelformat != SIXEL_PIXELFORMAT_RGBFLOAT32 ||
        frame->colorspace != SIXEL_COLORSPACE_GAMMA ||
        frame->ncolors != -1 || frame->transparent != -1 ||
        frame->transparent_mask != NULL || frame->transparent_mask_size ||
        frame->alpha_zero_is_transparent) {
        return SIXEL_BAD_INPUT;
    }
    memcpy(state->words, sixel_frame_get_pixels(frame), state->count * 4u);
    return SIXEL_OK;
}

static void
uf_reset(uint32_t bad, int early)
{
    uf_bad = bad;
    uf_early = early;
    uf_reads = uf_rows = uf_matched = uf_seen = uf_rgba = 0;
    uf_injected = 0;
}

#endif

int
tiff_float_failure_test(int argc, char **argv, int mode)
{
#if HAVE_LIBTIFF
    sixel_allocator_t *allocator;
    sixel_chunk_t *normal_chunk;
    sixel_chunk_t *bad_chunk;
    sixel_loader_component_t *component;
    uf_capture_t normal;
    uf_capture_t state;
    SIXELSTATUS status;
    void *object;
    size_t baseline;
    uint32_t bad;
    int rows;
    int valid;
    int qualified;
    int qualifier_ok;
    int behavior;
    int recovered;
    int no;
    int ok;

    if (argc != 3 || mode < 0 || mode > 4) { return 2; }
    allocator = NULL;
    normal_chunk = bad_chunk = NULL;
    component = NULL;
    object = NULL;
    no = 0;
    ok = 0;
    uf_live = 0u;
    bad = mode == 1 ? 0x7f800000u : mode == 2 ? 0xff800000u :
          mode == 4 ? 0u : 0x7fc12345u;
    uf_width = mode == 1 || mode == 2 ? 6 : 4;
    uf_height = mode == 1 || mode == 2 ? 4 : 2;
    uf_reset(0u, 0);
    status = sixel_allocator_new(&allocator, uf_malloc, uf_calloc,
                                  uf_realloc, uf_free);
    if (SIXEL_FAILED(status)) { goto cleanup; }
    status = sixel_chunk_create_from_source(&normal_chunk, argv[1], 0,
                                            NULL, allocator);
    if (SIXEL_FAILED(status)) { goto cleanup; }
    status = sixel_chunk_create_from_source(&bad_chunk, argv[2], 0,
                                            NULL, allocator);
    if (SIXEL_FAILED(status)) { goto cleanup; }
    status = uf_new(allocator, &object);
    if (SIXEL_FAILED(status)) { goto cleanup; }
    component = object;
    status = component->vtbl->setopt(component,
        SIXEL_LOADER_COMPONENT_OPTION_LIBTIFF_ENABLE_CMS, &no);
    if (SIXEL_FAILED(status)) { goto cleanup; }
    baseline = uf_live;
    memset(&normal, 0, sizeof(normal));
    status = component->vtbl->load(component, normal_chunk,
                                   uf_capture, &normal);
    rows = uf_rows;
    valid = status == SIXEL_OK && normal.calls == 1 &&
        uf_matched == 24 && uf_live == baseline && uf_rgba == 0;
    uf_reset(bad, mode == 3 ? 1 : mode == 4 ? 2 : 0);
    memset(&state, 0, sizeof(state));
    status = component->vtbl->load(component, bad_chunk, uf_capture, &state);
    qualified = uf_rows == rows && uf_reads == rows &&
                uf_matched == 24 && uf_seen == 1;
    if (mode == 4) {
        qualified = uf_rows == 1 && uf_reads == 2 && uf_matched == 12 &&
                    uf_seen == 0 && uf_injected == 1;
    }
    qualifier_ok = qualified;
    if (mode == 3) {
        /* Early rejection is a deliberate false positive for late failure. */
        qualifier_ok = !qualified && uf_rows == 0 && uf_matched == 0 &&
                    uf_seen == 0 && uf_injected == 1;
    }
    behavior = status == SIXEL_TIFF_ERROR && state.calls == 0 &&
               uf_rgba == 0 && uf_live == baseline;
    printf("UFFAULT|qualified=%d|reads=%d|rows=%d|matched=%d|last=%d\n",
        qualified, uf_reads, uf_rows, uf_matched, uf_seen);
    printf("UFRESULT|behavior=%d|status=%x|callbacks=%d|rgba=%d|"
        "live=%zu|baseline=%zu\n", behavior, status, state.calls, uf_rgba,
        uf_live, baseline);
    uf_reset(0u, 0);
    memset(&state, 0, sizeof(state));
    status = component->vtbl->load(component, normal_chunk, uf_capture, &state);
    recovered = status == SIXEL_OK && state.calls == 1 &&
        uf_rows == rows && uf_matched == 24 && uf_live == baseline &&
        uf_rgba == 0 && state.count == normal.count &&
        memcmp(state.words, normal.words, normal.count * 4u) == 0;
    printf("UFRECOVER|normal=%d|recovered=%d\n", valid, recovered);
    ok = valid && qualifier_ok && behavior && recovered;
cleanup:
    if (component != NULL) { component->vtbl->unref(component); }
    if (normal_chunk != NULL) { normal_chunk->vtbl->unref(normal_chunk); }
    if (bad_chunk != NULL) { bad_chunk->vtbl->unref(bad_chunk); }
    sixel_allocator_unref(allocator);
    printf("UFEND|live=%zu\n", uf_live);
    return ok && uf_live == 0u ? 0 : 1;
#else
    (void)argc;
    (void)argv;
    (void)mode;
    return 77;
#endif
}
