/* SPDX-License-Identifier: MIT */
/* Literal integer/coordinate oracles own source accuracy. The actual CMM
 * applied to that independent source qualifies adapter publication only;
 * CMM arithmetic remains covered by the shared numerical tests. */
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
#include "tiff_layout_test_common.h"

#if HAVE_LIBTIFF
#include <tiffio.h>
#include "tests/data/color-numeric/tiff-layout.h"

static struct {
    int fixture, engine, fault, target, armed, managed;
    int ok, calls, cms_calls, reads, tiles, rgba, attempts, injected, live;
    int width, height, format, space;
    size_t count;
    float expected[918];
    char path[4096];
} ti;

static void *
ti_malloc(size_t bytes)
{
    void *data;

    if (ti.armed) {
        ++ti.attempts;
        if (ti.fault >= 1 && ti.fault <= 3 &&
            ti.attempts == ti.fault) {
            ++ti.injected;
            return NULL;
        }
    }
    data = malloc(bytes);
    ti.live += data != NULL;
    return data;
}

static void *
ti_calloc(size_t count, size_t bytes)
{
    void *data;

    data = calloc(count, bytes);
    ti.live += data != NULL;
    return data;
}

static void *
ti_realloc(void *old, size_t bytes)
{
    void *data;
    int empty;

    empty = old == NULL;
    data = realloc(old, bytes);
    ti.live += empty && data != NULL;
    return data;
}

static void
ti_free(void *data)
{
    ti.live -= data != NULL;
    free(data);
}

static tmsize_t
ti_scanline_size(TIFF *tif)
{
    if (ti.fault == 4) { ++ti.injected; return 0; }
    return TIFFScanlineSize(tif);
}

static tmsize_t
ti_tile_size(TIFF *tif)
{
    if (ti.fault == 5) { ++ti.injected; return 0; }
    return TIFFTileSize(tif);
}

static tmsize_t
ti_tile_row(TIFF *tif)
{
    if (ti.fault == 6) { ++ti.injected; return 1; }
    return TIFFTileRowSize(tif);
}

static int
ti_read(TIFF *tif, void *row, uint32_t y, uint16_t plane)
{
    ++ti.reads;
    if ((ti.fault == 7 && ti.reads == 2) ||
        (ti.fault == 8 && ti.reads == 3)) {
        ++ti.injected;
        return -1;
    }
    return TIFFReadScanline(tif, row, y, plane);
}

static tmsize_t
ti_read_tile(TIFF *tif, void *tile, uint32_t x, uint32_t y,
              uint32_t z, uint16_t plane)
{
    tmsize_t bytes;

    ++ti.tiles;
    if (ti.fault == 9 && ti.tiles == 2) {
        ++ti.injected;
        return -1;
    }
    bytes = TIFFReadTile(tif, tile, x, y, z, plane);
    if (ti.fault == 10 && bytes > 0) {
        ++ti.injected;
        return bytes - 1;
    }
    return bytes;
}

static int
ti_rgba(TIFF *tif, uint32_t w, uint32_t h, uint32_t *data,
        int orientation, int stop)
{
    ++ti.rgba;
    return TIFFReadRGBAImageOriented(tif, w, h, data, orientation, stop);
}

static int
ti_linear(unsigned char *pixels, int width, int height, int format,
          sixel_cms_profile_t *profile)
{
    ++ti.cms_calls;
    ti.ok &= width == ti.width && height == ti.height &&
        format == SIXEL_PIXELFORMAT_RGBFLOAT32;
    ti.ok &= memcmp(pixels, ti_cases[ti.fixture].source,
                    ti.count * 3u * sizeof(float)) == 0;
    if (ti.fault == 11) {
        /* Exercise the real shared helper's failure result. Its internal
         * partial-apply rollback is owned by the existing shared/B1 tests;
         * this check owns the new adapter's whole-source publication. */
        ++ti.injected;
        return sixel_cms_convert_profile_to_linearrgb(pixels, width, height,
                                                      format, NULL);
    }
    return sixel_cms_convert_profile_to_linearrgb(pixels, width, height,
                                                  format, profile);
}

static int
ti_target_format(void)
{
    return ti.target == 1 ? SIXEL_PIXELFORMAT_OKLABFLOAT32
         : SIXEL_PIXELFORMAT_LINEARRGBFLOAT32;
}

#define g_sixel_loader_libtiff_vtbl ti_local_0
#define load_tiff ti_local_1
#define load_with_libtiff ti_local_2
#define loader_can_try_libtiff ti_local_3
#define sixel_loader_libtiff_component ti_local_4
#define sixel_loader_libtiff_component_t ti_local_5
#define sixel_loader_libtiff_load ti_local_6
#define sixel_loader_libtiff_name ti_local_7
#define sixel_loader_libtiff_new ti_local_8
#define sixel_loader_libtiff_placeholder ti_local_9
#define sixel_loader_libtiff_placeholder_function ti_local_10
#define sixel_loader_libtiff_predicate ti_local_11
#define sixel_loader_libtiff_ref ti_local_12
#define sixel_loader_libtiff_setopt ti_local_13
#define sixel_loader_libtiff_unref ti_local_14
#define tiff_clamp_lab_ab ti_local_15
#define tiff_clamp_unit ti_local_16
#define tiff_convert_embedded_icc_to_srgb ti_local_17
#define tiff_convert_embedded_icc_to_srgb_nolcms ti_local_19
#define tiff_decode_lab_u16_ab ti_local_21
#define tiff_decode_lab_u16_l ti_local_22
#define tiff_map_scanline_coordinates ti_local_23
#define tiff_memory_chunk ti_local_24
#define tiff_memory_chunk_t ti_local_25
#define tiff_memory_close ti_local_26
#define tiff_memory_map ti_local_27
#define tiff_memory_read ti_local_28
#define tiff_memory_seek ti_local_29
#define tiff_memory_size ti_local_30
#define tiff_memory_unmap ti_local_31
#define tiff_memory_write ti_local_32
#define tiff_normalize_lab_ab_sample ti_local_33
#define tiff_normalize_lab_l_sample ti_local_34
#define tiff_orientation_is_supported ti_local_35
#define tiff_photometric_allows_embedded_icc ti_local_36
#define tiff_try_load_high_precision ti_local_37
#define tiff_logluv_xyz_to_linear ti_local_38
#define tiff_convert_samples_to_linear ti_local_39
#define tiff_try_load_integer_alpha ti_local_40
#define tiff_read_separate_uint16 ti_separate
#define TIFFReadScanline ti_read
#define TIFFScanlineSize ti_scanline_size
#define TIFFReadTile ti_read_tile
#define TIFFTileSize ti_tile_size
#define TIFFTileRowSize ti_tile_row
#define TIFFReadRGBAImageOriented ti_rgba
#define sixel_cms_convert_profile_to_linearrgb ti_linear
#define loader_cms_target_pixelformat ti_target_format
#define tiff_publish_integer_alpha til_ns_11_0
#define tiff_integer_layout_t til_ns_11_1
#define tiff_scatter_integer_row til_ns_11_2
#define tiff_try_load_integer_layouts til_ns_11_3
#include "src/loader-libtiff.c"
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
#undef TIFFReadScanline
#undef TIFFScanlineSize
#undef TIFFReadTile
#undef TIFFTileSize
#undef TIFFTileRowSize
#undef TIFFReadRGBAImageOriented
#undef sixel_cms_convert_profile_to_linearrgb
#undef loader_cms_target_pixelformat


static SIXELSTATUS
ti_capture(sixel_frame_t *frame, void *context)
{
    size_t i;
    int mask;
    unsigned char expected;

    (void)context;
    ++ti.calls;
    ti.ok &= frame->width == ti.width && frame->height == ti.height &&
        frame->pixelformat == ti.format && frame->colorspace == ti.space;
    mask = ti_cases[ti.fixture].alpha != 0;
    ti.ok &= frame->transparent_mask_size == (mask ? ti.count : 0u) &&
        frame->alpha_zero_is_transparent == mask;
    if (!ti.ok) { return SIXEL_BAD_INPUT; }
    for (i = 0u; mask && i < ti.count; ++i) {
        ti.ok &= frame->transparent_mask[i] ==
            (ti_cases[ti.fixture].coverage[i] == 0u);
    }
    if (ti.format == SIXEL_PIXELFORMAT_RGB888) {
        for (i = 0u; i < ti.count * 3u; ++i) {
            expected = (unsigned char)((double)ti.expected[i] * 255.0 + 0.5);
            ti.ok &= frame->pixels.u8ptr[i] == expected;
        }
    } else {
        ti.ok &= memcmp(frame->pixels.u8ptr, ti.expected,
                        ti.count * 3u * sizeof(float)) == 0;
    }
    return ti.ok ? SIXEL_OK : SIXEL_BAD_INPUT;
}

static int
ti_reference(char const *path)
{
    TIFF *tif;
    void *bytes;
    uint32_t length;
    sixel_cms_profile_t *profile;
    SIXELSTATUS status;
    int negotiate;

    memcpy(ti.expected, ti_cases[ti.fixture].source,
           ti.count * 3u * sizeof(float));
    ti.format = SIXEL_PIXELFORMAT_RGBFLOAT32;
    ti.space = SIXEL_COLORSPACE_GAMMA;
    if (!ti.managed) {
        if (ti_cases[ti.fixture].bits <= 8 &&
            ti_cases[ti.fixture].photo != 3 && !ti_cases[ti.fixture].alpha) {
            ti.format = SIXEL_PIXELFORMAT_RGB888;
        }
        return 1;
    }
    tif = TIFFOpen(path, "r");
    if (tif == NULL) { return 0; }
    bytes = NULL;
    length = 0;
    (void)TIFFGetField(tif, TIFFTAG_ICCPROFILE, &length, &bytes);
    profile = sixel_cms_open_profile_from_mem(bytes, length);
    ti.ok &= profile != NULL && sixel_cms_convert_profile_to_linearrgb(
        (unsigned char *)ti.expected, ti.width, ti.height,
        SIXEL_PIXELFORMAT_RGBFLOAT32, profile);
    sixel_cms_close_profile(profile);
    TIFFClose(tif);
    ti.format = SIXEL_PIXELFORMAT_LINEARRGBFLOAT32;
    ti.space = SIXEL_COLORSPACE_LINEAR;
    negotiate = ti_cases[ti.fixture].bits <= 8 ||
        ti_cases[ti.fixture].photo == 3 || ti_cases[ti.fixture].alpha;
    if (ti.target && negotiate) {
        /* The frame contract uses extended intermediate coordinates;
         * the public byte-oriented storage converter clamps and is not an
         * appropriate reference for this target boundary. */
        status = sixel_color_convert_rgbf32(ti.expected, ti.count,
            SIXEL_COLORSPACE_LINEAR, SIXEL_COLORSPACE_OKLAB);
        ti.ok &= status == SIXEL_OK;
        ti.format = SIXEL_PIXELFORMAT_OKLABFLOAT32;
        ti.space = SIXEL_COLORSPACE_OKLAB;
    }
    return ti.ok;
}

static int
ti_phase(sixel_loader_component_t *component, sixel_chunk_t *chunk, int fault)
{
    SIXELSTATUS status;
    int live;
    int managed;

    live = ti.live;
    managed = ti.managed;
    if (fault == 11) { ti.managed = 0; }
    if (!ti_reference(ti.path)) { ti.managed = managed; return 0; }
    ti.managed = managed;
    ti.fault = fault;
    ti.calls = ti.cms_calls = ti.reads = ti.tiles = ti.rgba = 0;
    ti.attempts = ti.injected = 0;
    ti.armed = 1;
    fprintf(stderr, "TIL|begin|engine=%s|src=%d|applies=%d\n",
        sixel_cms_engine_to_string((sixel_cms_engine_t)ti.engine),
        ti_cases[ti.fixture].photo == 0 || ti_cases[ti.fixture].photo == 1
            ? SIXEL_CMS_PIXELFORMAT_GRAY_F32 : SIXEL_CMS_PIXELFORMAT_RGB_F32,
        ti.managed && fault == 0);
    status = component->vtbl->load(component, chunk, ti_capture, NULL);
    ti.armed = 0;
    ti.ok &= ti.live == live && ti.rgba == 0;
    if (fault > 0 && fault <= 10) {
        ti.ok &= SIXEL_FAILED(status) && ti.calls == 0 &&
            ti.cms_calls == 0 && ti.injected == 1;
    } else {
        ti.ok &= status == SIXEL_OK && ti.calls == 1 &&
            ti.cms_calls == ti.managed && ti.injected == (fault == 11);
    }
    fprintf(stderr, "TIL|fixture=%d|engine=%d|fault=%d|status=%x|"
        "calls=%d|cms=%d|reads=%d|tiles=%d|rgba=%d|injected=%d|ok=%d\n",
        ti.fixture, ti.engine, fault, status, ti.calls, ti.cms_calls,
        ti.reads, ti.tiles, ti.rgba, ti.injected, ti.ok);
    fprintf(stderr, "TIL|end|ok=%d\n", ti.ok);
    return ti.ok;
}
#endif

int
test_tiff_layout(char const *directory, int fixture, int engine,
                  int fault, int target)
{
#if HAVE_LIBTIFF
    sixel_allocator_t *allocator;
    sixel_loader_component_t *component;
    sixel_chunk_t *chunk;
    sixel_cms_engine_t saved;
    SIXELSTATUS status;
    unsigned char *original;
    void *object;
    char path[4096];
    size_t bytes;
    int enabled;

#if !HAVE_LCMS2
    if (engine == SIXEL_CMS_ENGINE_LCMS2) { return 77; }
#endif
#if !defined(__APPLE__) || !HAVE_COREGRAPHICS
    if (engine == SIXEL_CMS_ENGINE_COLORSYNC) { return 77; }
#endif
    allocator = NULL;
    component = NULL;
    chunk = NULL;
    original = NULL;
    object = NULL;
    status = SIXEL_FALSE;
    saved = sixel_cms_get_engine();
    sixel_cms_set_engine((sixel_cms_engine_t)engine);
    memset(&ti, 0, sizeof(ti));
    ti.ok = 1;
    ti.fixture = fixture;
    ti.engine = engine;
    ti.target = target;
    ti.width = ti_cases[fixture].orientation >= 5
        ? ti_cases[fixture].height : ti_cases[fixture].width;
    ti.height = ti_cases[fixture].orientation >= 5
        ? ti_cases[fixture].width : ti_cases[fixture].height;
    ti.count = (size_t)ti.width * ti.height;
    ti.managed = engine != SIXEL_CMS_ENGINE_NONE && ti_cases[fixture].matched;
    enabled = engine != SIXEL_CMS_ENGINE_NONE;
    snprintf(path, sizeof(path), "%s/%s", directory, ti_cases[fixture].file);
    memcpy(ti.path, path, sizeof(path));
    if (!ti_reference(path)) { goto cleanup; }
    status = sixel_allocator_new(&allocator, ti_malloc, ti_calloc,
                                  ti_realloc, ti_free);
    if (SIXEL_FAILED(status)) { goto cleanup; }
    status = sixel_chunk_create_from_source(&chunk, path, 0, NULL, allocator);
    if (SIXEL_FAILED(status)) { goto cleanup; }
    bytes = sixel_chunk_get_size(chunk);
    original = malloc(bytes);
    if (original == NULL) { status = SIXEL_BAD_ALLOCATION; goto cleanup; }
    memcpy(original, sixel_chunk_get_buffer(chunk), bytes);
    status = ti_local_8(allocator, &object);
    component = object;
    if (SIXEL_FAILED(status)) { goto cleanup; }
    status = component->vtbl->setopt(component,
        SIXEL_LOADER_COMPONENT_OPTION_LIBTIFF_ENABLE_CMS, &enabled);
    if (SIXEL_FAILED(status)) { goto cleanup; }
    if (!ti_phase(component, chunk, 0)) { goto cleanup; }
    if (fault) {
        if (!ti_phase(component, chunk, fault)) { goto cleanup; }
        if (!ti_phase(component, chunk, 0)) { goto cleanup; }
    }
    ti.ok &= memcmp(original, sixel_chunk_get_buffer(chunk), bytes) == 0;
cleanup:
    if (component != NULL) { component->vtbl->unref(component); }
    if (chunk != NULL) { chunk->vtbl->unref(chunk); }
    sixel_allocator_unref(allocator);
    free(original);
    sixel_cms_set_engine(saved);
    return status == SIXEL_OK && ti.ok && ti.live == 0 ? 0 : 1;
#else
    (void)directory;
    (void)fixture;
    (void)engine;
    (void)fault;
    (void)target;
    return 77;
#endif
}
