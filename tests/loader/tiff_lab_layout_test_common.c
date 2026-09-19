/* SPDX-License-Identifier: MIT */
/* Small memory-only fixtures protect the format contract, independently of
 * the production decoder and of live CMS-generated expected values. */
#include "config.h"
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "src/cms.h"
#include "src/frame-private.h"
#include "src/loader.h"
#include "src/loader-manager.h"
#include "src/loader-libtiff.h"
#include "src/chunk-factory.h"
#include "tiff_lab_layout_test_common.h"

#if HAVE_LIBTIFF
#include <tiffio.h>
#include "tests/data/color-numeric/tiff-lab-layout.h"

static struct {
    int index, mode, engine, ok, calls, applies, rgba, reads, injected;
    int allocations, fail_allocation, live;
} lx;

static void *
lx_malloc(size_t size)
{
    void *p;

    ++lx.allocations;
    if (lx.fail_allocation != 0 && lx.allocations == lx.fail_allocation) {
        ++lx.injected;
        return NULL;
    }
    p = malloc(size);
    lx.live += p != NULL;
    return p;
}

static void
lx_free(void *p)
{
    lx.live -= p != NULL;
    free(p);
}

static int
lx_linear(unsigned char *pixels, int w, int h, int format,
          unsigned char const *profile, size_t size)
{
    ++lx.applies;
    lx.ok &= w == 5 && h == 3 &&
        format == SIXEL_PIXELFORMAT_CIELABFLOAT32 &&
        memcmp(pixels, tlx_cases[lx.index].source, 45u * sizeof(float)) == 0;
    (void)profile;
    (void)size;
    /* Deliberately decline, without mutation. The caller must retain the
     * entire normalized Lab source for the format-specific fallback. */
    return 0;
}

static int
lx_scanline(TIFF *tif, void *buffer, uint32_t row, uint16_t plane)
{
    ++lx.reads;
    if (lx.mode == 6 && row == 2u && plane == 2u) {
        ++lx.injected;
        return -1;
    }
    return TIFFReadScanline(tif, buffer, row, plane);
}

static int
lx_rgba(TIFF *tif, uint32_t w, uint32_t h, uint32_t *raster,
        int orientation, int stop)
{
    ++lx.rgba;
    return TIFFReadRGBAImageOriented(tif, w, h, raster, orientation, stop);
}

/* Adapter-copy namespace for amalgamated test builds. Only dependency
 * observation/fault calls change; all production reading code is included. */
#define g_sixel_loader_libtiff_vtbl lx_ns_0
#define load_tiff lx_load
#define load_with_libtiff lx_ns_2
#define loader_can_try_libtiff lx_ns_3
#define sixel_loader_libtiff_component lx_ns_4
#define sixel_loader_libtiff_component_t lx_ns_5
#define sixel_loader_libtiff_load lx_ns_6
#define sixel_loader_libtiff_name lx_ns_7
#define sixel_loader_libtiff_new lx_ns_8
#define sixel_loader_libtiff_placeholder lx_ns_9
#define sixel_loader_libtiff_placeholder_function lx_ns_10
#define sixel_loader_libtiff_predicate lx_ns_11
#define sixel_loader_libtiff_ref lx_ns_12
#define sixel_loader_libtiff_setopt lx_ns_13
#define sixel_loader_libtiff_unref lx_ns_14
#define tiff_clamp_lab_ab lx_ns_15
#define tiff_clamp_unit lx_ns_16
#define tiff_convert_embedded_icc_to_srgb lx_ns_17
#define tiff_convert_embedded_icc_to_srgb_nolcms lx_ns_18
#define tiff_decode_lab_u16_ab lx_ns_19
#define tiff_decode_lab_u16_l lx_ns_20
#define tiff_map_scanline_coordinates lx_ns_21
#define tiff_memory_chunk lx_ns_22
#define tiff_memory_chunk_t lx_ns_23
#define tiff_memory_close lx_ns_24
#define tiff_memory_map lx_ns_25
#define tiff_memory_read lx_ns_26
#define tiff_memory_seek lx_ns_27
#define tiff_memory_size lx_ns_28
#define tiff_memory_unmap lx_ns_29
#define tiff_memory_write lx_ns_30
#define tiff_normalize_lab_ab_sample lx_ns_31
#define tiff_normalize_lab_l_sample lx_ns_32
#define tiff_orientation_is_supported lx_ns_33
#define tiff_photometric_allows_embedded_icc lx_ns_34
#define tiff_try_load_high_precision lx_ns_35
#define tiff_logluv_xyz_to_linear lx_ns_36
#define tiff_convert_samples_to_linear lx_ns_37
#define tiff_try_load_integer_alpha lx_ns_38
#define tiff_read_separate_uint16 lx_ns_39
#define tiff_publish_integer_alpha lx_ns_40
#define tiff_integer_layout_t lx_ns_41
#define tiff_scatter_integer_row lx_ns_42
#define tiff_try_load_integer_layouts lx_ns_43
#define tiff_lab8_ranges lx_ns_44
#define tiff_lab8_to_linear lx_ns_45
#define TIFFReadScanline lx_scanline
#define TIFFReadRGBAImageOriented lx_rgba
#define sixel_cms_convert_to_linearrgb_with_profile_bytes lx_linear
#include "src/loader-libtiff.c"
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

static int
lx_values(float const *pixels, int managed)
{
    double value, scale, lo, hi;
    size_t i, c;
    int checked;

    checked = 0;
    for (i = 0u; i < 15u; ++i) {
        scale = 0.0;
        for (c = 0u; c < 3u; ++c) {
            scale = fmax(scale, fabs(tlx_cases[lx.index].fallback[i][c]));
        }
        for (c = 0u; c < 3u; ++c) {
            value = pixels[i * 3u + c];
            if (!isfinite(value)) { return 0; }
            /* Existing engine contracts do not bound native CMM behavior
             * at the extreme encoded chroma boundary. Exact normalization
             * above and unmanaged RGB values below still cover every word. */
            if (managed && lx.engine != 2 &&
                (fabs(tlx_cases[lx.index].source[i][1]) > 0.9 ||
                 fabs(tlx_cases[lx.index].source[i][2]) > 0.9)) {
                continue;
            }
            if (managed) {
                lo = tlx_cases[lx.index].bounds[i][lx.engine - 2][0][c];
                hi = tlx_cases[lx.index].bounds[i][lx.engine - 2][1][c];
            } else {
                lo = tlx_cases[lx.index].fallback[i][c] -
                    scale * 64.0 / 16777216.0;
                hi = tlx_cases[lx.index].fallback[i][c] +
                    scale * 64.0 / 16777216.0;
            }
            if (value < lo || value > hi) {
                fprintf(stderr, "TLX|case=%d|pixel=%zu|c=%zu|v=%.17g|"
                    "lo=%.17g|hi=%.17g\n", lx.index, i, c, value, lo, hi);
                return 0;
            }
            ++checked;
        }
    }
    return checked >= 27;
}

static double
lx_encode(double v)
{
    double a;

    a = fabs(v);
    a = a <= 0.0031308 ? a * 12.92 :
        1.055 * pow(a, 1.0 / 2.4) - 0.055;
    return v < 0.0 ? -a : a;
}

static SIXELSTATUS
lx_capture(sixel_frame_t *frame, void *context)
{
    int linear;
    size_t i, c;
    double expected, value, scale;

    (void)context;
    ++lx.calls;
    if (lx.mode == 10 || lx.mode == 11) {
        lx.ok &= frame->width == 5 && frame->height == 3 &&
            frame->pixelformat == (lx.mode == 10
                ? SIXEL_PIXELFORMAT_RGBFLOAT32 : SIXEL_PIXELFORMAT_RGB888);
        if (!lx.ok) { return SIXEL_BAD_INPUT; }
        for (i = 0u; i < 15u; ++i) {
            scale = 0.0;
            for (c = 0u; c < 3u; ++c) {
                scale = fmax(scale,
                    fabs(lx_encode(tlx_cases[lx.index].fallback[i][c])));
            }
            for (c = 0u; c < 3u; ++c) {
                expected = lx_encode(tlx_cases[lx.index].fallback[i][c]);
                if (lx.mode == 10) {
                    value = ((float *)frame->pixels.u8ptr)[i * 3u + c];
                    lx.ok &= fabs(value - expected) <=
                        scale * 64.0 / 16777216.0;
                } else {
                    expected = floor(fmax(0.0, fmin(1.0, expected)) *
                                     255.0 + 0.5);
                    lx.ok &= frame->pixels.u8ptr[i * 3u + c] == expected;
                }
            }
        }
        return lx.ok ? SIXEL_OK : SIXEL_BAD_INPUT;
    }
    linear = lx.mode >= 2 && lx.mode <= 4;
    linear |= tlx_cases[lx.index].photo != 8;
    lx.ok &= frame->width == 5 && frame->height == 3 &&
        frame->pixelformat == (linear ? SIXEL_PIXELFORMAT_LINEARRGBFLOAT32
                                      : SIXEL_PIXELFORMAT_RGBFLOAT32) &&
        frame->colorspace == (linear ? SIXEL_COLORSPACE_LINEAR
                                     : SIXEL_COLORSPACE_GAMMA) &&
        frame->transparent_mask == NULL && frame->ncolors == -1;
    lx.ok &= lx_values((float const *)sixel_frame_get_pixels(frame),
                       lx.mode >= 2 && lx.mode <= 4);
    return lx.ok ? SIXEL_OK : SIXEL_BAD_INPUT;
}

static int
lx_public(unsigned char const *data, size_t size)
{
    sixel_allocator_t *allocator;
    sixel_chunk_t *chunk;
    sixel_loader_t *loader;
    SIXELSTATUS status;
    char order[128];
    int ok;

    allocator = NULL;
    chunk = NULL;
    loader = NULL;
    ok = 0;
    snprintf(order, sizeof(order),
        "libtiff:cms_engine=%s:cms_target=%s:prefer_8bit=%d!",
        lx.mode == 1 || lx.mode >= 10 ? "none" :
        sixel_cms_engine_to_string((sixel_cms_engine_t)lx.engine),
        lx.mode >= 10 ? "gamma" : "linear", lx.mode == 11);
    status = sixel_allocator_new(&allocator, NULL, NULL, NULL, NULL);
    if (SIXEL_FAILED(status)) { goto cleanup; }
    status = sixel_chunk_create_from_memory(&chunk, data, size, NULL,
                                             allocator);
    if (SIXEL_FAILED(status)) { goto cleanup; }
    status = sixel_loader_new(&loader, NULL);
    if (SIXEL_FAILED(status)) { goto cleanup; }
    sixel_loader_set_prefer_float32(loader, lx.mode != 11);
    status = sixel_loader_setopt(loader, SIXEL_LOADER_OPTION_LOADER_ORDER,
                                 order);
    if (SIXEL_FAILED(status)) { goto cleanup; }
    status = sixel_loader_load_chunk(loader, chunk, lx_capture);
    ok = SIXEL_SUCCEEDED(status) && lx.calls == 1 && lx.ok;
cleanup:
    if (!ok) {
        fprintf(stderr, "TLX|public-status=%x|calls=%d|message=%s\n",
            status, lx.calls, sixel_helper_get_additional_message());
    }
    sixel_loader_unref(loader);
    if (chunk != NULL) { chunk->vtbl->unref(chunk); }
    sixel_allocator_unref(allocator);
    return ok;
}

static int
lx_direct(unsigned char const *data, size_t size)
{
    sixel_allocator_t *allocator;
    unsigned char *pixels, *mask;
    SIXELSTATUS status;
    int w, h, format, target, ok;

    allocator = NULL;
    pixels = mask = NULL;
    w = h = format = target = 0;
    lx.allocations = -1;
    if (SIXEL_FAILED(sixel_allocator_new(&allocator, lx_malloc,
        NULL, NULL, lx_free))) { return 0; }
    status = lx_load(&pixels, data, size, &w, &h, &format, allocator,
                     1, &target, &mask, NULL);
    if (lx.mode >= 5 && lx.mode <= 7) {
        ok = SIXEL_FAILED(status) && pixels == NULL && mask == NULL &&
            lx.rgba == 0 && lx.applies == 0 &&
            (lx.mode == 5 || lx.injected == 1);
    } else {
        ok = SIXEL_SUCCEEDED(status) && w == 5 && h == 3 &&
            mask == NULL && lx.applies == 1 && lx.rgba == 0 && lx.ok &&
            format == (lx.index >= 3 ? SIXEL_PIXELFORMAT_RGBFLOAT32
                                    : SIXEL_PIXELFORMAT_LINEARRGBFLOAT32) &&
            lx_values((float const *)pixels, 0);
    }
    sixel_allocator_free(allocator, pixels);
    sixel_allocator_free(allocator, mask);
    sixel_allocator_unref(allocator);
    return ok && lx.live == 0;
}
#endif

int
tiff_lab_layout_test(int index, int mode)
{
#if HAVE_LIBTIFF
    sixel_cms_engine_t saved;
    unsigned char data[4096];
    size_t size, entry, offset;
    uint32_t word;
    unsigned int tag;
    int big;
    int ok, first, last, i, variant, variants;

    if (!loader_manager_entry_available("libtiff")) { return 77; }
    memset(&lx, 0, sizeof(lx));
    saved = sixel_cms_get_engine();
    lx.engine = mode == 3 ? 3 : mode == 4 ? 4 : 2;
    sixel_cms_set_engine((sixel_cms_engine_t)lx.engine);
    if ((int)sixel_cms_get_engine() != lx.engine) {
        sixel_cms_set_engine(saved);
        return 77;
    }
    first = index < 0 ? 0 : index;
    last = index < 0 ? (mode >= 10 ? 2 : 5) : index;
    ok = 1;
    variants = mode == 7 ? 2 : mode == 8 ? 3 : mode == 5 ? 2 : 1;
    for (i = first; ok && i <= last; ++i) {
        for (variant = 1; ok && variant <= variants; ++variant) {
            lx.index = i;
            lx.mode = mode;
            lx.ok = 1;
            lx.calls = lx.applies = lx.rgba = lx.reads = lx.injected = 0;
            lx.allocations = lx.live = 0;
            lx.fail_allocation = mode == 7 ? variant : 0;
            size = tlx_cases[i].size;
            if (size > sizeof(data)) { ok = 0; break; }
            memcpy(data, tlx_cases[i].bytes, size);
            if (mode == 5 || mode == 8) {
                /* The custom Decode witness is big endian. Retain a
                 * well-formed SRATIONAL array but give it only four items. */
                big = data[0] == 'M';
                for (entry = 10u; entry + 12u <= size; entry += 12u) {
                    tag = big ? data[entry] * 256u + data[entry + 1u]
                              : data[entry] + data[entry + 1u] * 256u;
                    if (tag == (mode == 5 ? 433u : 34675u)) { break; }
                }
                if (entry + 12u > size) { ok = 0; break; }
                word = 0u;
                for (offset = 0u; offset < 4u; ++offset) {
                    word |= (uint32_t)data[entry + 8u + offset] <<
                        ((big ? 3u - offset : offset) * 8u);
                }
                if (mode == 5 && variant == 1) {
                    data[entry + (big ? 7u : 4u)] = 4u;
                } else if (mode == 5) {
                    /* Reverse the L minimum/maximum pair. */
                    if (word + 16u > size) { ok = 0; break; }
                    memcpy(data + word, data + word + 8u, 8u);
                } else if (variant == 1) {
                    /* Remove the ICC tag without changing source samples. */
                    data[entry] = data[entry + 1u] = 0xfdu;
                } else {
                    if (word + 40u > size) { ok = 0; break; }
                    if (variant == 2) {
                        /* Truncate to an inline value: this cannot contain
                         * an ICC header, regardless of parser leniency. */
                        memset(data + entry + 4u, 0, 4u);
                        data[entry + (big ? 7u : 4u)] = 4u;
                        memcpy(data + entry + 8u, "bad!", 4u);
                    } else {
                        memcpy(data + word + 16u, "RGB ", 4u);
                    }
                }
            }
            ok = (mode >= 1 && mode <= 4) || mode >= 8
                ? lx_public(data, size) : lx_direct(data, size);
            if (mode != 5 && mode != 8) {
                ok &= memcmp(data, tlx_cases[i].bytes, size) == 0;
            }
            if (ok && (mode == 6 || mode == 7)) {
                /* Retry the same borrowed bytes after failure: no partial
                 * result, retained allocation or poisoned decoder state. */
                lx.mode = 0;
                lx.fail_allocation = 0;
                lx.applies = lx.rgba = 0;
                ok = lx_direct(data, size);
            }
        }
    }
    sixel_cms_set_engine(saved);
    fprintf(stderr, "TLX|index=%d|mode=%d|engine=%d|ok=%d\n",
        index, mode, lx.engine, ok);
    return ok ? 0 : 1;
#else
    (void)index;
    (void)mode;
    return 77;
#endif
}
