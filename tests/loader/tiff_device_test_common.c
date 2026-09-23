/* SPDX-License-Identifier: MIT */
/* Literal source coordinates and independent PCS intervals are the oracle.
 * A private adapter copy observes the actual source/CMS boundary and injects
 * faults without replacing production color conversion or publication. */
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
#include "tiff_device_test_common.h"

#if HAVE_LIBTIFF
#include <tiffio.h>
#include "tests/data/color-numeric/tiff-device.h"

static struct {
    int fixture, engine, mode, fault, target, background;
    int ok, calls, cms, rgba, reads, injected, allocations, live, armed;
    int source_floats;
} td;

static void *
td_malloc(size_t size)
{
    void *p;

    if (td.armed &&
        ((++td.allocations == td.fault && td.fault <= 3) ||
         (td.fault == 8 && size == 45u * sizeof(float)))) {
        ++td.injected;
        return NULL;
    }
    if (td.armed && size == 60u * sizeof(float)) { ++td.source_floats; }
    p = malloc(size);
    td.live += p != NULL;
    return p;
}

static void *
td_calloc(size_t count, size_t size)
{
    void *p;

    p = td_malloc(count * size);
    if (p != NULL) { memset(p, 0, count * size); }
    return p;
}

static void *
td_realloc(void *old, size_t size)
{
    void *p;
    int empty;

    empty = old == NULL;
    p = realloc(old, size);
    td.live += empty && p != NULL;
    return p;
}

static void
td_free(void *p)
{
    td.live -= p != NULL;
    free(p);
}

static int
td_lab(unsigned char *pixels, int w, int h, int format,
       unsigned char const *profile, size_t size)
{
    ++td.cms;
    td.ok &= w * h == 15 && format == SIXEL_PIXELFORMAT_CIELABFLOAT32 &&
        memcmp(pixels, td_cases[td.fixture].source, 45u * sizeof(float)) == 0;
    if (td.fault == 6) { ++td.injected; return 0; }
    return sixel_cms_convert_to_linearrgb_with_profile_bytes(
        pixels, w, h, format, profile, size);
}

static int
td_apply(sixel_cms_transform_t const *transform, void const *source,
         void *output, size_t count)
{
    int ok;

    ++td.cms;
    td.ok &= count == 15u && memcmp(source, td_cases[td.fixture].source,
                                    60u * sizeof(float)) == 0;
    td.ok &= (int)sixel_cms_transform_engine(transform) == td.engine;
    /* First perform a real prefix conversion, then fail after destination
     * mutation. Only the untouched four-component source can recover. */
    ok = sixel_cms_do_transform(transform, source, output,
                                td.fault == 6 ? 1u : count);
    if (td.fault == 6) { td.ok &= ok; ++td.injected; return 0; }
    if (td.fault == 7) {
        td.ok &= ok;
        ++td.injected;
        ((float *)output)[count * 3u - 1u] = NAN;
    }
    return ok;
}

static int
td_row(TIFF *tif, void *data, uint32_t y, uint16_t plane)
{
    ++td.reads;
    if (td.fault == 4 && td.reads == 3) {
        ++td.injected;
        return -1;
    }
    return TIFFReadScanline(tif, data, y, plane);
}

static tmsize_t
td_tile(TIFF *tif, void *data, uint32_t x, uint32_t y,
        uint32_t z, uint16_t plane)
{
    tmsize_t size;

    ++td.reads;
    size = TIFFReadTile(tif, data, x, y, z, plane);
    if (td.fault == 5 && td.reads == 2 && size > 0) {
        ++td.injected;
        return size - 1;
    }
    return size;
}

static int
td_rgba(TIFF *tif, uint32_t w, uint32_t h, uint32_t *data,
         int orientation, int stop)
{
    ++td.rgba;
    return TIFFReadRGBAImageOriented(tif, w, h, data, orientation, stop);
}

static int
td_target(void)
{
    return td.target == 1 ? SIXEL_PIXELFORMAT_RGBFLOAT32 :
        td.target == 2 ? SIXEL_PIXELFORMAT_RGB888 :
        SIXEL_PIXELFORMAT_LINEARRGBFLOAT32;
}
#define g_sixel_loader_libtiff_vtbl td_ns_0
#define load_tiff td_load
#define load_with_libtiff td_ns_2
#define loader_can_try_libtiff td_ns_3
#define sixel_loader_libtiff_component td_ns_4
#define sixel_loader_libtiff_component_t td_ns_5
#define sixel_loader_libtiff_load td_ns_6
#define sixel_loader_libtiff_name td_ns_7
#define sixel_loader_libtiff_new td_ns_8
#define sixel_loader_libtiff_placeholder td_ns_9
#define sixel_loader_libtiff_placeholder_function td_ns_10
#define sixel_loader_libtiff_predicate td_ns_11
#define sixel_loader_libtiff_ref td_ns_12
#define sixel_loader_libtiff_setopt td_ns_13
#define sixel_loader_libtiff_unref td_ns_14
#define tiff_clamp_lab_ab td_ns_15
#define tiff_clamp_unit td_ns_16
#define tiff_convert_embedded_icc_to_srgb td_ns_17
#define tiff_convert_embedded_icc_to_srgb_nolcms td_ns_18
#define tiff_decode_lab_u16_ab td_ns_19
#define tiff_decode_lab_u16_l td_ns_20
#define tiff_map_scanline_coordinates td_ns_21
#define tiff_memory_chunk td_ns_22
#define tiff_memory_chunk_t td_ns_23
#define tiff_memory_close td_ns_24
#define tiff_memory_map td_ns_25
#define tiff_memory_read td_ns_26
#define tiff_memory_seek td_ns_27
#define tiff_memory_size td_ns_28
#define tiff_memory_unmap td_ns_29
#define tiff_memory_write td_ns_30
#define tiff_normalize_lab_ab_sample td_ns_31
#define tiff_normalize_lab_l_sample td_ns_32
#define tiff_orientation_is_supported td_ns_33
#define tiff_photometric_allows_embedded_icc td_ns_34
#define tiff_try_load_high_precision td_ns_35
#define tiff_logluv_xyz_to_linear td_ns_36
#define tiff_convert_samples_to_linear td_ns_37
#define tiff_try_load_integer_alpha td_ns_38
#define tiff_read_separate_uint16 td_ns_39
#define tiff_publish_integer_alpha td_ns_40
#define tiff_integer_layout_t td_ns_41
#define tiff_scatter_integer_row td_ns_42
#define tiff_try_load_integer_layouts td_ns_43
#define tiff_lab8_ranges td_ns_44
#define tiff_lab8_to_linear td_ns_45
#define TIFFReadScanline td_row
#define TIFFReadRGBAImageOriented td_rgba
#define sixel_cms_convert_to_linearrgb_with_profile_bytes td_lab
#define tiff_convert_cmyk_to_linear td_ink_linear
#define sixel_cms_do_transform td_apply
#define TIFFReadTile td_tile
#define loader_cms_target_pixelformat td_target
#include "src/loader-libtiff.c"
#undef loader_cms_target_pixelformat
#undef TIFFReadTile
#undef sixel_cms_do_transform
#undef tiff_convert_cmyk_to_linear
#undef sixel_cms_convert_to_linearrgb_with_profile_bytes
#undef TIFFReadRGBAImageOriented
#undef TIFFReadScanline
#undef tiff_lab8_to_linear
#undef tiff_lab8_ranges
#undef tiff_try_load_integer_layouts
#undef tiff_scatter_integer_row
#undef tiff_integer_layout_t
#undef tiff_publish_integer_alpha
#undef tiff_read_separate_uint16
#undef tiff_try_load_integer_alpha
#undef tiff_convert_samples_to_linear
#undef tiff_logluv_xyz_to_linear
#undef tiff_try_load_high_precision
#undef tiff_photometric_allows_embedded_icc
#undef tiff_orientation_is_supported
#undef tiff_normalize_lab_l_sample
#undef tiff_normalize_lab_ab_sample
#undef tiff_memory_write
#undef tiff_memory_unmap
#undef tiff_memory_size
#undef tiff_memory_seek
#undef tiff_memory_read
#undef tiff_memory_map
#undef tiff_memory_close
#undef tiff_memory_chunk_t
#undef tiff_memory_chunk
#undef tiff_map_scanline_coordinates
#undef tiff_decode_lab_u16_l
#undef tiff_decode_lab_u16_ab
#undef tiff_convert_embedded_icc_to_srgb_nolcms
#undef tiff_convert_embedded_icc_to_srgb
#undef tiff_clamp_unit
#undef tiff_clamp_lab_ab
#undef sixel_loader_libtiff_unref
#undef sixel_loader_libtiff_setopt
#undef sixel_loader_libtiff_ref
#undef sixel_loader_libtiff_predicate
#undef sixel_loader_libtiff_placeholder_function
#undef sixel_loader_libtiff_placeholder
#undef sixel_loader_libtiff_new
#undef sixel_loader_libtiff_name
#undef sixel_loader_libtiff_load
#undef sixel_loader_libtiff_component_t
#undef sixel_loader_libtiff_component
#undef loader_can_try_libtiff
#undef load_with_libtiff
#undef load_tiff
#undef g_sixel_loader_libtiff_vtbl

static double
td_decode(double v)
{
    return v <= 0.04045 ? v / 12.92 : pow((v + 0.055) / 1.055, 2.4);
}

static double
td_encode(double v)
{
    double a;

    a = fabs(v);
    a = a <= 0.0031308 ? 12.92 * a : 1.055 * pow(a, 1.0/2.4) - 0.055;
    return v < 0.0 ? -a : a;
}

static SIXELSTATUS
td_capture(sixel_frame_t *frame, void *context)
{
    size_t i, c;
    int managed, linear, bytes, mask, composed;
    double lo, hi, value, scale, coverage, background, rounding;
    unsigned char const *data;
    float const *pixels;

    (void)context;
    ++td.calls;
    managed = td.mode == 0 && td.fault != 6 && td.fault != 7 &&
        td.fault != 8;
    linear = managed || (td_cases[td.fixture].photo != 5 &&
                           td_cases[td.fixture].bits == 8);
    composed = td.background && td_cases[td.fixture].alpha &&
        td.fixture != 14;
    bytes = linear ? td.target == 2 :
        td_cases[td.fixture].photo == 5 && td_cases[td.fixture].bits == 8 &&
        !composed && (td_cases[td.fixture].alpha != 1 || td.fixture == 14);
    mask = td_cases[td.fixture].alpha && td.fixture != 14;
    td.ok &= frame->width == (td_cases[td.fixture].orientation >= 5 ? 3 : 5)
        && frame->height == (td_cases[td.fixture].orientation >= 5 ? 5 : 3)
        && frame->pixelformat == (bytes ? SIXEL_PIXELFORMAT_RGB888 :
            linear && !td.target ? SIXEL_PIXELFORMAT_LINEARRGBFLOAT32 :
            SIXEL_PIXELFORMAT_RGBFLOAT32)
        && frame->colorspace == (linear && !td.target
            ? SIXEL_COLORSPACE_LINEAR : SIXEL_COLORSPACE_GAMMA)
        && frame->transparent_mask_size == (mask ? 15u : 0u);
    if (!td.ok) {
        fprintf(stderr, "TD|metadata|format=%x|space=%d|mask=%zu\n",
            frame->pixelformat, frame->colorspace,
            frame->transparent_mask_size);
        return SIXEL_BAD_INPUT;
    }
    data = sixel_frame_get_pixels(frame);
    pixels = (float const *)data;
    for (i = 0u; i < 15u; ++i) {
        if (mask) {
            td.ok &= frame->transparent_mask[i] ==
                (td_cases[td.fixture].coverage[i] == 0u);
        }
        scale = 0.0;
        for (c = 0u; c < 3u; ++c) {
            scale = fmax(scale, fabs(td_cases[td.fixture].fallback[i][c]));
        }
        coverage = (double)td_cases[td.fixture].coverage[i] /
            (td_cases[td.fixture].bits == 8 ? 255.0 : 65535.0);
        for (c = 0u; c < 3u; ++c) {
            lo = managed ? td_cases[td.fixture].bounds[i][td.engine-2][0][c]
                : td_cases[td.fixture].fallback[i][c] -
                  scale * 64.0 / 16777216.0;
            hi = managed ? td_cases[td.fixture].bounds[i][td.engine-2][1][c]
                : td_cases[td.fixture].fallback[i][c] +
                  scale * 64.0 / 16777216.0;
            if (composed && coverage > 0.0 && coverage < 1.0) {
                background = td_decode((double)(64u * (c + 1u)) / 255.0);
                lo = coverage * (linear ? lo : td_decode(lo)) +
                    (1.0 - coverage) * background;
                hi = coverage * (linear ? hi : td_decode(hi)) +
                    (1.0 - coverage) * background;
                if (!linear) { lo = td_encode(lo); hi = td_encode(hi); }
            }
            if (linear && td.target) { lo = td_encode(lo); hi = td_encode(hi); }
            /* A source interval shrinks with coverage. Include the final
             * F32 store after composition and (if requested) transfer;
             * these rounding errors do not shrink with source alpha. */
            if (composed || (linear && td.target)) {
                rounding = fmax(fabs(lo), fabs(hi)) * 2.0 / 16777216.0;
                lo -= rounding;
                hi += rounding;
            }
            if (bytes) {
                lo = floor(fmax(0.0, fmin(1.0, lo)) * 255.0 + 0.5);
                hi = floor(fmax(0.0, fmin(1.0, hi)) * 255.0 + 0.5);
            }
            value = bytes ? data[i*3u+c] : pixels[i*3u+c];
            if (!isfinite(value) || value < lo || value > hi) {
                fprintf(stderr, "TD|f=%d|e=%d|i=%zu|c=%zu|v=%.12g|"
                    "lo=%.12g|hi=%.12g\n", td.fixture, td.engine,
                    i, c, value, lo, hi);
                td.ok = 0;
                return SIXEL_BAD_INPUT;
            }
        }
    }
    return td.ok ? SIXEL_OK : SIXEL_BAD_INPUT;
}

static int
td_phase(sixel_loader_component_t *component, sixel_chunk_t *chunk, int fault)
{
    SIXELSTATUS status;
    int live;

    live = td.live;
    td.fault = fault;
    td.calls = td.cms = td.rgba = td.reads = td.injected = td.allocations = 0;
    td.source_floats = 0;
    td.armed = 1;
    status = component->vtbl->load(component, chunk, td_capture, NULL);
    td.armed = 0;
    td.ok &= live == td.live && td.rgba == 0;
    if (fault == 1 || fault == 2 || fault == 3 || fault == 4 || fault == 5) {
        td.ok &= SIXEL_FAILED(status) && td.calls == 0 && td.cms == 0;
    } else {
        td.ok &= status == SIXEL_OK && td.calls == 1;
    }
    if (td.mode == 1 || td.mode == 2) {
        td.ok &= td.cms == 0;
        if (td_cases[td.fixture].photo == 5 &&
            td_cases[td.fixture].bits == 8 &&
            td_cases[td.fixture].alpha != 1 && !td.background) {
            td.ok &= td.source_floats == 0;
        }
    }
    if (fault) { td.ok &= td.injected == 1; }
    if (td.mode == 0 && !fault) { td.ok &= td.cms == 1; }
    fprintf(stderr, "TD|fixture=%d|engine=%d|mode=%d|fault=%d|calls=%d|"
        "cms=%d|reads=%d|rgba=%d|live=%d|status=%x|ok=%d\n",
        td.fixture, td.engine, td.mode, fault, td.calls, td.cms, td.reads,
        td.rgba, td.live-live, status, td.ok);
    return td.ok;
}
#endif

int
tiff_device_test(int fixture, int engine, int mode, int fault,
                  int target, int background)
{
#if HAVE_LIBTIFF
    static unsigned char const bg[3] = {64, 128, 192};
    sixel_allocator_t *allocator;
    sixel_loader_component_t *component;
    sixel_chunk_t *chunk;
    sixel_cms_engine_t saved;
    SIXELSTATUS status;
    void *object;
    int enabled, variant;

    saved = sixel_cms_get_engine();
    sixel_cms_set_engine((sixel_cms_engine_t)engine);
    if ((int)sixel_cms_get_engine() != engine) {
        sixel_cms_set_engine(saved);
        return 77;
    }
    memset(&td, 0, sizeof(td));
    td.ok = 1;
    td.fixture = fixture;
    td.engine = engine;
    td.mode = mode;
    td.target = target;
    td.background = background;
    allocator = NULL;
    component = NULL;
    chunk = NULL;
    object = NULL;
    enabled = mode != 1;
    variant = mode < 2 ? 0 : mode-1;
    status = sixel_allocator_new(&allocator, td_malloc, td_calloc,
                                  td_realloc, td_free);
    if (SIXEL_FAILED(status)) { goto cleanup; }
    status = sixel_chunk_create_from_memory(&chunk,
        td_cases[fixture].bytes[variant], td_cases[fixture].sizes[variant],
        NULL, allocator);
    if (SIXEL_FAILED(status)) { goto cleanup; }
    status = td_ns_8(allocator, &object);
    if (SIXEL_FAILED(status)) { goto cleanup; }
    component = object;
    status = component->vtbl->setopt(component,
        SIXEL_LOADER_COMPONENT_OPTION_LIBTIFF_ENABLE_CMS, &enabled);
    if (SIXEL_FAILED(status)) { goto cleanup; }
    if (background) {
        status = component->vtbl->setopt(component,
            SIXEL_LOADER_OPTION_BGCOLOR, bg);
        if (SIXEL_FAILED(status)) { goto cleanup; }
    }
    if (!td_phase(component, chunk, 0)) { goto cleanup; }
    if (fault && (!td_phase(component, chunk, fault) ||
                  !td_phase(component, chunk, 0))) { goto cleanup; }
cleanup:
    td.ok &= SIXEL_SUCCEEDED(status);
    if (component != NULL) { component->vtbl->unref(component); }
    if (chunk != NULL) { chunk->vtbl->unref(chunk); }
    sixel_allocator_unref(allocator);
    sixel_cms_set_engine(saved);
    return td.ok && td.live == 0 ? 0 : 1;
#else
    (void)fixture; (void)engine; (void)mode; (void)fault;
    (void)target; (void)background;
    return 77;
#endif
}
