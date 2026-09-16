/* SPDX-License-Identifier: MIT */
/* Test-only hooks qualify real decoder progress before checking publication.
 * The included adapter has an isolated namespace and no production switch. */
#include "config.h"
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "src/allocator.h"
#include "src/chunk.h"
#include "src/chunk-factory.h"
#include "src/frame-private.h"
#include "src/loader.h"
#include "src/loader-libtiff.h"
#include "tests/data/color-numeric/tiff-logluv.h"
#include "tiff_logluv_test_common.h"
#if HAVE_LIBTIFF
#include <tiffio.h>

static int tlf_mode;
static int tlf_mutated;
static int tlf_injected;
static int tlf_allocations;
static int tlf_reads;
static int tlf_native_rows;
static int tlf_first_matches;
static int tlf_rgba_calls;
static int tlf_targets;
static int tlf_target_matches;
static size_t tlf_live;
static float tlf_first[12];
static float tlf_normal[24];

static void *
tlf_malloc(size_t size)
{
    void *p;

    p = malloc(size);
    if (p != NULL) { ++tlf_live; }
    return p;
}

static void *
tlf_calloc(size_t n, size_t size)
{
    void *p;

    p = calloc(n, size);
    if (p != NULL) { ++tlf_live; }
    return p;
}

static void *
tlf_realloc(void *ptr, size_t size)
{
    void *p;
    int empty;

    empty = ptr == NULL;
    p = realloc(ptr, size);
    if (p != NULL && empty) { ++tlf_live; }
    return p;
}

static void
tlf_free(void *ptr)
{
    if (ptr != NULL) { --tlf_live; }
    free(ptr);
}

static int
tlf_set(TIFF *tif, uint32_t tag, ...)
{
    va_list args;
    int ok;

    va_start(args, tag);
    ok = TIFFVSetField(tif, tag, args);
    va_end(args);
    if (tag == TIFFTAG_SGILOGDATAFMT && ok) {
        ++tlf_mutated;
        if (tlf_mode == 1) { ++tlf_injected; return 0; }
    }
    return ok;
}

static int
tlf_get(TIFF *tif, uint32_t tag, ...)
{
    va_list args;
    va_list copy;
    uint16_t *value;
    int ok;

    va_start(args, tag);
    va_copy(copy, args);
    ok = TIFFVGetFieldDefaulted(tif, tag, args);
    if (tlf_mode == 2 && tlf_mutated && tag == TIFFTAG_BITSPERSAMPLE && ok) {
        value = va_arg(copy, uint16_t *);
        *value = 16u;
        ++tlf_injected;
    }
    va_end(copy);
    va_end(args);
    return ok;
}

static tmsize_t
tlf_scanline_size(TIFF *tif)
{
    tmsize_t size;

    size = TIFFScanlineSize(tif);
    if (tlf_mode == 3 && tlf_mutated && size == 48) {
        ++tlf_injected;
        return 0;
    }
    return size;
}

static void *
tlf_allocate(sixel_allocator_t *allocator, size_t size)
{
    if (tlf_mutated) {
        ++tlf_allocations;
        if ((tlf_mode == 4 && tlf_allocations == 1 && size == 48u) ||
            (tlf_mode == 5 && tlf_allocations == 2 && size == 96u)) {
            ++tlf_injected;
            return NULL;
        }
    }
    return sixel_allocator_malloc(allocator, size);
}

static int
tlf_read(TIFF *tif, void *data, uint32_t row, uint16_t sample)
{
    int ok;

    ++tlf_reads;
    if (tlf_mode == 6 && row == 1u && tlf_native_rows == 1) {
        ++tlf_injected;
        return -1;
    }
    ok = TIFFReadScanline(tif, data, row, sample);
    if (ok == 1) {
        ++tlf_native_rows;
        if (row == 0u) {
            if (tlf_mode == 0) { memcpy(tlf_first, data, sizeof(tlf_first)); }
            tlf_first_matches = memcmp(tlf_first, data, sizeof(tlf_first)) == 0;
        }
        if (row == 1u && (tlf_mode == 7 || tlf_mode == 8)) {
            ((float *)data)[11] = tlf_mode == 7 ? NAN : INFINITY;
            ++tlf_injected;
        }
    }
    return ok;
}

static int
tlf_rgba(TIFF *tif, uint32_t w, uint32_t h, uint32_t *raster,
          int orientation, int stop)
{
    ++tlf_rgba_calls;
    return TIFFReadRGBAImageOriented(tif, w, h, raster, orientation, stop);
}

static SIXELSTATUS
tlf_target(sixel_frame_t *frame, int format)
{
    ++tlf_targets;
    tlf_target_matches = frame->width == 4 && frame->height == 2 &&
        frame->pixelformat == SIXEL_PIXELFORMAT_LINEARRGBFLOAT32 &&
        frame->colorspace == SIXEL_COLORSPACE_LINEAR &&
        memcmp(sixel_frame_get_pixels(frame), tlf_normal,
               sizeof(tlf_normal)) == 0;
    if (tlf_mode == 9 && tlf_target_matches) {
        ++tlf_injected;
        return SIXEL_BAD_INPUT;
    }
    return sixel_frame_set_pixelformat(frame, format);
}

#define g_sixel_loader_libtiff_vtbl tlf_local_0
#define load_tiff tlf_local_1
#define load_with_libtiff tlf_local_2
#define loader_can_try_libtiff tlf_local_3
#define sixel_loader_libtiff_component tlf_local_4
#define sixel_loader_libtiff_component_t tlf_local_5
#define sixel_loader_libtiff_load tlf_local_6
#define sixel_loader_libtiff_name tlf_local_7
#define sixel_loader_libtiff_new tlf_loader_new
#define sixel_loader_libtiff_placeholder tlf_local_9
#define sixel_loader_libtiff_placeholder_function tlf_local_10
#define sixel_loader_libtiff_predicate tlf_local_11
#define sixel_loader_libtiff_ref tlf_local_12
#define sixel_loader_libtiff_setopt tlf_local_13
#define sixel_loader_libtiff_unref tlf_local_14
#define tiff_clamp_lab_ab tlf_local_15
#define tiff_clamp_unit tlf_local_16
#define tiff_convert_embedded_icc_to_srgb tlf_local_17
#define tiff_convert_embedded_icc_to_srgb_float32 tlf_local_18
#define tiff_convert_embedded_icc_to_srgb_nolcms tlf_local_19
#define tiff_convert_rgbf32_gamma_to_linear tlf_local_20
#define tiff_decode_lab_u16_ab tlf_local_21
#define tiff_decode_lab_u16_l tlf_local_22
#define tiff_logluv_xyz_to_linear tlf_local_23
#define tiff_map_scanline_coordinates tlf_local_24
#define tiff_memory_chunk tlf_local_25
#define tiff_memory_chunk_t tlf_local_26
#define tiff_memory_close tlf_local_27
#define tiff_memory_map tlf_local_28
#define tiff_memory_read tlf_local_29
#define tiff_memory_seek tlf_local_30
#define tiff_memory_size tlf_local_31
#define tiff_memory_unmap tlf_local_32
#define tiff_memory_write tlf_local_33
#define tiff_normalize_lab_ab_sample tlf_local_34
#define tiff_normalize_lab_l_sample tlf_local_35
#define tiff_orientation_is_supported tlf_local_36
#define tiff_photometric_allows_embedded_icc tlf_local_37
#define tiff_try_load_high_precision tlf_local_38
#define TIFFSetField tlf_set
#define TIFFGetFieldDefaulted tlf_get
#define TIFFScanlineSize tlf_scanline_size
#define TIFFReadScanline tlf_read
#define TIFFReadRGBAImageOriented tlf_rgba
#define sixel_allocator_malloc tlf_allocate
#define sixel_frame_set_pixelformat tlf_target
#define tiff_convert_byte_samples_to_linear luf_byte
#include "src/loader-libtiff.c"
#undef tiff_convert_byte_samples_to_linear
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
#undef TIFFSetField
#undef TIFFGetFieldDefaulted
#undef TIFFScanlineSize
#undef TIFFReadScanline
#undef TIFFReadRGBAImageOriented
#undef sixel_allocator_malloc
#undef sixel_frame_set_pixelformat

typedef struct tlf_capture_state {
    int calls;
    float pixels[24];
} tlf_capture_state_t;

static SIXELSTATUS
tlf_capture(sixel_frame_t *frame, void *context)
{
    tlf_capture_state_t *state;
    float const *pixels;
    size_t i;
    size_t c;
    double value;

    state = context;
    ++state->calls;
    if (frame->width != 4 || frame->height != 2 ||
        frame->pixelformat != SIXEL_PIXELFORMAT_LINEARRGBFLOAT32 ||
        frame->colorspace != SIXEL_COLORSPACE_LINEAR ||
        frame->ncolors != -1 || frame->transparent != -1 ||
        frame->transparent_mask != NULL || frame->transparent_mask_size ||
        frame->alpha_zero_is_transparent) {
        return SIXEL_BAD_INPUT;
    }
    pixels = (float const *)sixel_frame_get_pixels(frame);
    for (i = 0u; i < 8u; ++i) {
        for (c = 0u; c < 3u; ++c) {
            value = pixels[i * 3u + c];
            if (!isfinite(value) || value < tlog_bounds[0][0][i][0][c] ||
                value > tlog_bounds[0][0][i][1][c]) {
                return SIXEL_BAD_INPUT;
            }
        }
    }
    memcpy(state->pixels, pixels, sizeof(state->pixels));
    return SIXEL_OK;
}

static void
tlf_reset(int mode)
{
    tlf_mode = mode;
    tlf_mutated = 0;
    tlf_injected = 0;
    tlf_allocations = 0;
    tlf_reads = 0;
    tlf_native_rows = 0;
    tlf_first_matches = 0;
    tlf_rgba_calls = 0;
    tlf_targets = 0;
    tlf_target_matches = 0;
}
#endif

int
tiff_logluv_failure_test(int argc, char **argv, int mode)
{
#if HAVE_LIBTIFF
    sixel_allocator_t *allocator;
    sixel_chunk_t *chunk;
    sixel_loader_component_t *component;
    tlf_capture_state_t state;
    SIXELSTATUS status;
    void *object;
    size_t baseline;
    int normal;
    int qualified;
    int failed;
    int recovered;
    int ok;

    if (argc != 2) { return 2; }
    allocator = NULL;
    chunk = NULL;
    component = NULL;
    object = NULL;
    ok = 0;
    tlf_live = 0u;
    tlf_reset(0);
    memset(tlf_first, 0, sizeof(tlf_first));
    memset(tlf_normal, 0, sizeof(tlf_normal));
    status = sixel_allocator_new(&allocator, tlf_malloc, tlf_calloc,
                                  tlf_realloc, tlf_free);
    if (SIXEL_FAILED(status)) { goto cleanup; }
    status = sixel_chunk_create_from_source(&chunk, argv[1], 0,
                                            NULL, allocator);
    if (SIXEL_FAILED(status)) { goto cleanup; }
    status = tlf_loader_new(allocator, &object);
    if (SIXEL_FAILED(status)) { goto cleanup; }
    component = object;
    baseline = tlf_live;
    memset(&state, 0, sizeof(state));
    status = component->vtbl->load(component, chunk, tlf_capture, &state);
    normal = status == SIXEL_OK && state.calls == 1 &&
             tlf_mutated == 1 && tlf_allocations == 2 &&
             tlf_native_rows == 2 && tlf_rgba_calls == 0 &&
             tlf_targets == 1 && tlf_live == baseline;
    memcpy(tlf_normal, state.pixels, sizeof(tlf_normal));

    tlf_reset(mode);
    memset(&state, 0, sizeof(state));
    status = component->vtbl->load(component, chunk, tlf_capture, &state);
    qualified = tlf_mutated == 1 && tlf_injected == 1;
    if (mode <= 3) { qualified = qualified && tlf_allocations == 0; }
    if (mode == 4) { qualified = qualified && tlf_allocations == 1; }
    if (mode >= 5) { qualified = qualified && tlf_allocations == 2; }
    if (mode <= 5) { qualified = qualified && tlf_reads == 0; }
    if (mode >= 6) {
        qualified = qualified && tlf_reads == 2 && tlf_first_matches &&
            tlf_native_rows == (mode == 6 ? 1 : 2);
    }
    if (mode == 9) {
        qualified = qualified && tlf_targets == 1 && tlf_target_matches;
    } else {
        qualified = qualified && tlf_targets == 0;
    }
    /* Check publication even if fault qualification failed. */
    failed = SIXEL_FAILED(status) && state.calls == 0 &&
             tlf_rgba_calls == 0 && tlf_live == baseline;
    fprintf(stderr, "TLOGFAULT|mode=%d|qualified=%d|injected=%d|"
        "rows=%d|first=%d|targets=%d|target-values=%d\n", mode, qualified,
        tlf_injected, tlf_native_rows, tlf_first_matches,
        tlf_targets, tlf_target_matches);
    fprintf(stderr, "TLOGRESULT|failed=%d|status=%x|callbacks=%d|"
        "rgba=%d|live=%zu|baseline=%zu\n", failed, status,
        state.calls, tlf_rgba_calls, tlf_live, baseline);
    ok = normal && qualified && failed;

    tlf_reset(0);
    memset(&state, 0, sizeof(state));
    status = component->vtbl->load(component, chunk, tlf_capture, &state);
    recovered = status == SIXEL_OK && state.calls == 1 &&
        tlf_mutated == 1 && tlf_native_rows == 2 && tlf_targets == 1 &&
        tlf_rgba_calls == 0 && tlf_live == baseline &&
        memcmp(state.pixels, tlf_normal, sizeof(tlf_normal)) == 0;
    fprintf(stderr, "TLOGRECOVER|normal=%d|recovered=%d\n", normal, recovered);
    ok = ok && recovered;
cleanup:
    tlf_reset(0);
    if (component != NULL) { component->vtbl->unref(component); }
    if (chunk != NULL) { chunk->vtbl->unref(chunk); }
    sixel_allocator_unref(allocator);
    ok = ok && tlf_live == 0u;
    memset(tlf_first, 0, sizeof(tlf_first));
    memset(tlf_normal, 0, sizeof(tlf_normal));
    return ok ? 0 : 1;
#else
    (void)argc;
    (void)argv;
    (void)mode;
    return 77;
#endif
}
