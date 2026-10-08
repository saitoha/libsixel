/* SPDX-License-Identifier: MIT */
/* Independent values for the public dither/encode float-palette contract. */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sixel.h>
#include "src/dither.h"

/* A countdown spans both construction and output-cache allocation. */
static int direct_fail_after = -1;
static int direct_fault_hits;

static int
direct_deny(void)
{
    if (direct_fail_after < 0) {
        return 0;
    }
    if (direct_fail_after-- == 0) {
        ++direct_fault_hits;
        return 1;
    }
    return 0;
}

static void *
direct_malloc(size_t size)
{
    return direct_deny() ? NULL : malloc(size);
}

static void *
direct_calloc(size_t count, size_t size)
{
    return direct_deny() ? NULL : calloc(count, size);
}

static void *
direct_realloc(void *pointer, size_t size)
{
    return direct_deny() ? NULL : realloc(pointer, size);
}

typedef struct direct_capture {
    char bytes[4096];
    size_t length;
    int fail;
} direct_capture_t;

static int
direct_write(char *bytes, int length, void *data)
{
    direct_capture_t *capture;

    capture = data;
    if (capture->fail || length < 0 ||
            (size_t)length >= sizeof(capture->bytes) - capture->length) {
        return -1;
    }
    memcpy(capture->bytes + capture->length, bytes, (size_t)length);
    capture->length += (size_t)length;
    capture->bytes[capture->length] = '\0';
    return length;
}

static SIXELSTATUS
direct_init(sixel_dither_t *dither, void *pixels, int format)
{
    sixel_dither_set_diffusion_type(dither, SIXEL_DIFFUSE_NONE);
    sixel_dither_set_lut_policy(dither, SIXEL_LUT_POLICY_NONE);
    sixel_dither_set_final_merge(dither, SIXEL_FINAL_MERGE_NONE);
    return sixel_dither_initialize(dither, pixels, 2, 1, format,
        SIXEL_LARGE_AUTO, SIXEL_REP_AUTO, SIXEL_QUALITY_FULL);
}

static int
direct_wire(sixel_dither_t *dither, sixel_output_t *output,
            direct_capture_t *capture, int hls, int dark)
{
    float pixels[] = {.0149f, .0149f, .0149f, .5149f, .5149f, .5149f};
    unsigned char bytes[] = {4, 4, 4, 131, 131, 131};
    char expected[40];
    SIXELSTATUS status;

    memset(capture, 0, sizeof(*capture));
    sixel_output_set_palette_type(output,
        hls ? SIXEL_PALETTETYPE_HLS : SIXEL_PALETTETYPE_RGB);
    status = sixel_encode(dither->pixelformat == SIXEL_PIXELFORMAT_RGB888
        ? bytes : (unsigned char *)pixels, 2, 1, 0, dither, output);
    /* Gray 0.0149 has L=1.49%, safely inside the 1% rounding bin.
     * RGB8 roundtrip gives 4/255 (1.5686%), independently requiring 2%.
     * Float32 representation error is <0.00001 percentage points here.
     * The second gray is 51.49%, safely in bin 51 for both routes. */
    if (hls) {
        (void)snprintf(expected, sizeof(expected), ";1;0;%d;0", dark);
        return SIXEL_SUCCEEDED(status) &&
            strstr(capture->bytes, expected) != NULL &&
            strstr(capture->bytes, ";1;0;51;0") != NULL;
    }
    (void)snprintf(expected, sizeof(expected), ";2;%d;%d;%d",
        dark, dark, dark);
    return SIXEL_SUCCEEDED(status) &&
        strstr(capture->bytes, expected) != NULL &&
        strstr(capture->bytes, ";2;51;51;51") != NULL;
}

/* Independent sRGB transfer: .002 * 12.92 = .02584 -> 3%;
 * 1.055 * pow(.25, 1/2.4) - .055 = .5370987305 -> 54%.
 * Even the nearest boundary is >0.08 percentage points away; float input
 * and double conversion error is <0.00001 percentage points here.
 * Byte reassignment deliberately gives 1/255 -> .0498400898 -> 5%,
 * or 4/255 -> .1318038033 -> 13%, in the same linear source domain. */
static int
direct_linear_wire(sixel_dither_t *dither, sixel_output_t *output,
                   direct_capture_t *capture, int hls, int dark)
{
    float pixels[] = {.002f, .002f, .002f, .25f, .25f, .25f};
    char expected[40];
    SIXELSTATUS status;

    memset(capture, 0, sizeof(*capture));
    sixel_output_set_palette_type(output,
        hls ? SIXEL_PALETTETYPE_HLS : SIXEL_PALETTETYPE_RGB);
    status = sixel_encode((unsigned char *)pixels, 2, 1, 0, dither, output);
    if (hls) {
        (void)snprintf(expected, sizeof(expected), ";1;0;%d;0", dark);
        return SIXEL_SUCCEEDED(status) &&
            strstr(capture->bytes, expected) != NULL &&
            strstr(capture->bytes, ";1;0;54;0") != NULL;
    }
    (void)snprintf(expected, sizeof(expected), ";2;%d;%d;%d",
        dark, dark, dark);
    return SIXEL_SUCCEEDED(status) &&
        strstr(capture->bytes, expected) != NULL &&
        strstr(capture->bytes, ";2;54;54;54") != NULL;
}

static int
direct_lookup(sixel_dither_t *dither, float sample, float expected)
{
    float pixel[3];
    sixel_index_t *indices;
    sixel_palette_float32_entries_view_t view;
    int ok;

    pixel[0] = pixel[1] = pixel[2] = sample;
    indices = sixel_dither_apply_palette(dither, (unsigned char *)pixel, 1, 1);
    memset(&view, 0, sizeof(view));
    ok = indices != NULL && SIXEL_SUCCEEDED(
        dither->palette->vtbl->get_entries_float32(dither->palette, &view));
    ok = ok && view.entries != NULL && indices[0] < view.entry_count &&
        isfinite(view.entries[indices[0] * 3]) &&
        fabs((double)view.entries[indices[0] * 3] - expected) < 1e-7;
    sixel_allocator_free(dither->allocator, indices);
    return ok;
}

/* The mutable legacy getter itself is the contract under test. */
#if HAVE_DIAGNOSTIC_DEPRECATED_DECLARATIONS
# pragma GCC diagnostic push
# pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif
int
test_palette_direct_float(int argc, char **argv)
{
    float pixels[] = {.0149f, .0149f, .0149f, .5149f, .5149f, .5149f};
    float linear[] = {.002f, .002f, .002f, .25f, .25f, .25f};
    float *source;
    float extended[] = {-.25f, 1.25f, .0149f, .75f, .25f, .5149f};
    float gray[] = {.1f, .1f, .1f, .11f, .11f, .11f};
    float endpoints[] = {0, 0, 0, 1, 1, 1};
    float fs_pixels[] = {.4f, .4f, .4f, .4f, .4f, .4f, .4f, .4f, .4f};
    unsigned char bytes[] = {4, 4, 4, 131, 131, 131};
    unsigned char linear_bytes[] = {1, 1, 1, 64, 64, 64};
    unsigned char replacement[] = {0, 0, 0, 255, 255, 255};
    unsigned char *exposed;
    sixel_index_t *indices;
    sixel_allocator_t *allocator;
    sixel_dither_t *dither;
    sixel_dither_t *other;
    sixel_output_t *output;
    sixel_palette_float32_entries_view_t view;
    direct_capture_t capture;
    SIXELSTATUS status;
    unsigned int i;
    unsigned int j;
    unsigned int seen;
    int mode;
    int format;
    int hit;
    int ok;

    allocator = NULL;
    dither = NULL;
    other = NULL;
    output = NULL;
    indices = NULL;
    ok = 0;
    memset(&view, 0, sizeof(view));
    memset(&capture, 0, sizeof(capture));
    if (argc != 2) {
        return EXIT_FAILURE;
    }
    mode = atoi(argv[1]);
    source = mode >= 22 ? linear : pixels;
    format = mode >= 22 ? SIXEL_PIXELFORMAT_LINEARRGBFLOAT32
                        : SIXEL_PIXELFORMAT_RGBFLOAT32;
    if (SIXEL_FAILED(sixel_allocator_new(&allocator, direct_malloc,
            direct_calloc, direct_realloc, free)) ||
            SIXEL_FAILED(sixel_dither_new(&dither, 2, allocator)) ||
            SIXEL_FAILED(sixel_output_new(&output, direct_write, &capture,
                allocator)) ||
            SIXEL_FAILED(direct_init(dither, source, format))) {
        goto end;
    }
    switch (mode) {
    case 1:
    case 22:
        if (SIXEL_FAILED(direct_init(dither, extended, format)) || SIXEL_FAILED(
                dither->palette->vtbl->get_entries_float32(dither->palette,
                    &view)) || view.entries == NULL || view.entry_count != 2) {
            break;
        }
        /* No palette-order assumption. One retained sample per occupied
         * color; 1e-7 covers scale/unscale stores, not a byte roundtrip. */
        seen = 0;
        for (i = 0; i < 2; ++i) {
            for (j = 0; j < 2; ++j) {
                if (isfinite(view.entries[i * 3]) &&
                        isfinite(view.entries[i * 3 + 1]) &&
                        isfinite(view.entries[i * 3 + 2]) &&
                        fabs(view.entries[i * 3] - extended[j * 3]) < 1e-7 &&
                        fabs(view.entries[i * 3 + 1] -
                            extended[j * 3 + 1]) < 1e-7 &&
                        fabs(view.entries[i * 3 + 2] -
                            extended[j * 3 + 2]) < 1e-7) {
                    seen |= 1U << j;
                }
            }
        }
        ok = seen == 3 && dither->use_rgb101_palette;
        break;
    case 2:
    case 3:
        ok = direct_wire(dither, output, &capture, mode == 3, 1);
        break;
    case 4:
        ok = SIXEL_SUCCEEDED(direct_init(dither, gray,
            SIXEL_PIXELFORMAT_RGBFLOAT32)) &&
            direct_lookup(dither, .1055f, .11f);
        break;
    case 5:
        exposed = sixel_dither_get_palette(dither);
        ok = exposed != NULL && direct_wire(dither, output, &capture, 0, 1)
            && dither->use_rgb101_palette;
        break;
    case 6:
        exposed = sixel_dither_get_palette(dither);
        if (exposed == NULL) { break; }
        memcpy(exposed, replacement, sizeof(replacement));
        ok = direct_lookup(dither, .3f, 0) && !dither->use_rgb101_palette;
        break;
    case 7:
        sixel_dither_set_palette(dither, bytes);
        ok = direct_wire(dither, output, &capture, 0, 2) &&
            !dither->use_rgb101_palette;
        break;
    case 8:
        ok = SIXEL_SUCCEEDED(direct_init(dither, bytes,
            SIXEL_PIXELFORMAT_RGB888)) && !dither->use_rgb101_palette &&
            direct_wire(dither, output, &capture, 0, 2);
        break;
    case 9:
        ok = direct_wire(dither, output, &capture, 0, 1) &&
            SIXEL_SUCCEEDED(direct_init(dither, bytes,
                SIXEL_PIXELFORMAT_RGB888)) &&
            direct_wire(dither, output, &capture, 0, 2) &&
            SIXEL_SUCCEEDED(direct_init(dither, pixels,
                SIXEL_PIXELFORMAT_RGBFLOAT32)) &&
            direct_wire(dither, output, &capture, 0, 1);
        break;
    case 10:
        ok = SIXEL_SUCCEEDED(sixel_dither_new(&other, 2, allocator)) &&
            SIXEL_SUCCEEDED(direct_init(other, bytes,
                SIXEL_PIXELFORMAT_RGB888)) &&
            direct_wire(other, output, &capture, 0, 2) &&
            direct_wire(dither, output, &capture, 0, 1) &&
            direct_wire(other, output, &capture, 0, 2);
        break;
    case 11:
    case 35:
        ok = 1;
        for (i = 0; i < 128 && ok; ++i) {
            direct_fault_hits = 0;
            direct_fail_after = (int)i;
            status = direct_init(dither, source, format);
            direct_fail_after = -1;
            hit = direct_fault_hits;
            if (hit == 0) { break; }
            memset(&capture, 0, sizeof(capture));
            ok = SIXEL_FAILED(status) && SIXEL_FAILED(sixel_encode(
                (unsigned char *)source, 2, 1, 0, dither, output)) &&
                capture.length == 0 &&
                SIXEL_SUCCEEDED(direct_init(dither, source, format)) &&
                (mode >= 22
                    ? direct_linear_wire(dither, output, &capture, 0, 3)
                    : direct_wire(dither, output, &capture, 0, 1));
        }
        ok = ok && i > 0 && i < 128;
        break;
    case 12:
    case 38:
        source[0] = NAN;
        status = direct_init(dither, source, format);
        source[0] = mode >= 22 ? .002f : .0149f;
        ok = SIXEL_FAILED(status) && SIXEL_FAILED(sixel_encode(
            (unsigned char *)source, 2, 1, 0, dither, output)) &&
            capture.length == 0 &&
            SIXEL_SUCCEEDED(direct_init(dither, source, format)) &&
            (mode >= 22
                ? direct_linear_wire(dither, output, &capture, 0, 3)
                : direct_wire(dither, output, &capture, 0, 1));
        break;
    case 13:
        sixel_dither_set_pixelformat(dither,
            SIXEL_PIXELFORMAT_LINEARRGBFLOAT32);
        status = sixel_encode((unsigned char *)pixels, 2, 1, 0, dither, output);
        sixel_dither_set_pixelformat(dither, SIXEL_PIXELFORMAT_RGBFLOAT32);
        ok = SIXEL_FAILED(status) && capture.length == 0 &&
            direct_wire(dither, output, &capture, 0, 1);
        break;
    case 14:
        sixel_dither_set_pixelformat(dither, SIXEL_PIXELFORMAT_RGB888);
        ok = direct_wire(dither, output, &capture, 0, 1) &&
            dither->use_rgb101_palette;
        break;
    case 15:
        ok = direct_wire(dither, output, &capture, 0, 1) &&
            direct_wire(dither, output, &capture, 1, 1) &&
            direct_wire(dither, output, &capture, 0, 1);
        break;
    case 16:
        if (SIXEL_FAILED(direct_init(dither, endpoints,
                SIXEL_PIXELFORMAT_RGBFLOAT32)) || SIXEL_FAILED(
                sixel_dither_set_clamp_policy(dither,
                    SIXEL_DITHER_CLAMP_NONE, 1.01))) { break; }
        sixel_dither_set_diffusion_type(dither, SIXEL_DIFFUSE_FS);
        indices = sixel_dither_apply_palette(dither,
            (unsigned char *)fs_pixels, 3, 1);
        /* LTR FS: .4 -> black; .4+.4*7/16=.575 -> white;
         * .4+(.575-1)*7/16=.2140625 -> black. No ties or row taps. */
        if (indices == NULL || SIXEL_FAILED(
                dither->palette->vtbl->get_entries_float32(dither->palette,
                    &view)) || view.entries == NULL) { break; }
        ok = indices[0] < view.entry_count &&
            indices[1] < view.entry_count && indices[2] < view.entry_count &&
            view.entries[indices[0] * 3] == 0 &&
            view.entries[indices[1] * 3] == 1 &&
            view.entries[indices[2] * 3] == 0;
        break;
    case 17:
        exposed = sixel_dither_get_palette(dither);
        if (exposed == NULL) { break; }
        exposed[0] = exposed[1] = exposed[2] = 0;
        ok = direct_wire(dither, output, &capture, 0, 0) &&
            !dither->use_rgb101_palette;
        break;
    case 18:
        sixel_dither_set_palette(dither, replacement);
        ok = direct_lookup(dither, .3f, 0) && !dither->use_rgb101_palette;
        break;
    case 19:
        direct_fail_after = 0;
        status = direct_init(dither, pixels, SIXEL_PIXELFORMAT_RGBFLOAT32);
        direct_fail_after = -1;
        sixel_dither_set_palette(dither, bytes);
        ok = SIXEL_FAILED(status) &&
            direct_wire(dither, output, &capture, 0, 2);
        break;
    case 20:
        sixel_dither_set_diffusion_type(dither, SIXEL_DIFFUSE_INTERFRAME);
        status = sixel_encode((unsigned char *)pixels, 2, 1, 0, dither, output);
        sixel_dither_set_diffusion_type(dither, SIXEL_DIFFUSE_NONE);
        ok = SIXEL_FAILED(status) && capture.length == 0 &&
            direct_wire(dither, output, &capture, 0, 1);
        break;
    case 21:
        status = sixel_dither_initialize(dither, (unsigned char *)pixels,
            2, 1, SIXEL_PIXELFORMAT_RGBFLOAT32, SIXEL_LARGE_AUTO,
            SIXEL_REP_AUTO, SIXEL_QUALITY_HIGHCOLOR);
        ok = SIXEL_SUCCEEDED(status) && !dither->use_rgb101_palette &&
            SIXEL_SUCCEEDED(direct_init(dither, pixels,
                SIXEL_PIXELFORMAT_RGBFLOAT32)) &&
            direct_wire(dither, output, &capture, 0, 1);
        break;
    case 23:
    case 24:
        ok = direct_linear_wire(dither, output, &capture, mode == 24, 3);
        break;
    case 25:
        gray[0] = gray[1] = gray[2] = .002f;
        gray[3] = gray[4] = gray[5] = .004f;
        /* Midpoint .003: .0028 selects .002. Both centers quantize to
         * the same linear byte, so byte reconstruction cannot pass. */
        ok = SIXEL_SUCCEEDED(direct_init(dither, gray, format)) &&
            direct_lookup(dither, .0028f, .002f);
        break;
    case 26:
        direct_fault_hits = 0;
        direct_fail_after = 0;
        exposed = sixel_dither_get_palette(dither);
        direct_fail_after = -1;
        ok = direct_fault_hits == 0 && exposed != NULL &&
            memcmp(exposed, linear_bytes, sizeof(linear_bytes)) == 0 &&
            direct_linear_wire(dither, output, &capture, 0, 3);
        break;
    case 27:
        exposed = sixel_dither_get_palette(dither);
        if (exposed == NULL) { break; }
        exposed[0] = exposed[1] = exposed[2] = 4;
        ok = direct_lookup(dither, .01f, 4.0f / 255.0f) &&
            direct_linear_wire(dither, output, &capture, 0, 13);
        break;
    case 28:
        sixel_dither_set_palette(dither, linear_bytes);
        ok = direct_linear_wire(dither, output, &capture, 0, 5);
        break;
    case 29:
        replacement[3] = replacement[4] = replacement[5] = 128;
        sixel_dither_set_palette(dither, replacement);
        /* Stored byte coordinates are linear: the midpoint is 64/255,
         * not an sRGB transfer decode of 128/255. */
        ok = direct_lookup(dither, .3f, 128.0f / 255.0f);
        break;
    case 30:
        ok = direct_linear_wire(dither, output, &capture, 0, 3) &&
            SIXEL_SUCCEEDED(direct_init(dither, pixels,
                SIXEL_PIXELFORMAT_RGBFLOAT32)) &&
            direct_wire(dither, output, &capture, 0, 1) &&
            SIXEL_SUCCEEDED(direct_init(dither, bytes,
                SIXEL_PIXELFORMAT_RGB888)) &&
            direct_wire(dither, output, &capture, 0, 2) &&
            SIXEL_SUCCEEDED(direct_init(dither, linear, format)) &&
            direct_linear_wire(dither, output, &capture, 0, 3);
        break;
    case 31:
    case 32:
        sixel_dither_set_pixelformat(dither, mode == 31
            ? SIXEL_PIXELFORMAT_RGBFLOAT32 : SIXEL_PIXELFORMAT_RGB888);
        status = sixel_encode((unsigned char *)linear, 2, 1, 0, dither, output);
        sixel_dither_set_pixelformat(dither, format);
        ok = SIXEL_FAILED(status) && capture.length == 0 &&
            direct_linear_wire(dither, output, &capture, 0, 3);
        break;
    case 33:
        ok = SIXEL_SUCCEEDED(sixel_dither_new(&other, 2, allocator)) &&
            SIXEL_SUCCEEDED(direct_init(other, pixels,
                SIXEL_PIXELFORMAT_RGBFLOAT32)) &&
            direct_wire(other, output, &capture, 0, 1) &&
            direct_linear_wire(dither, output, &capture, 0, 3) &&
            direct_wire(other, output, &capture, 0, 1) &&
            direct_linear_wire(dither, output, &capture, 1, 3);
        break;
    case 34:
        gray[0] = gray[1] = gray[2] = .1f;
        gray[3] = gray[4] = gray[5] = .6f;
        for (i = 0; i < 9; ++i) { fs_pixels[i] = .3f; }
        if (SIXEL_FAILED(direct_init(dither, gray, format)) || SIXEL_FAILED(
                sixel_dither_set_clamp_policy(dither,
                    SIXEL_DITHER_CLAMP_NONE, 1.01))) { break; }
        sixel_dither_set_diffusion_type(dither, SIXEL_DIFFUSE_FS);
        indices = sixel_dither_apply_palette(dither,
            (unsigned char *)fs_pixels, 3, 1);
        /* LTR linear FS: .3 -> .1; .3+(.3-.1)*7/16=.3875 -> .6;
         * .3+(.3875-.6)*7/16=.20703125 -> .1. The emitted gamma
         * values .34919 and .79774 must not replace these centers. */
        if (indices == NULL || SIXEL_FAILED(
                dither->palette->vtbl->get_entries_float32(dither->palette,
                    &view)) || view.entries == NULL) { break; }
        ok = indices[0] < view.entry_count &&
            indices[1] < view.entry_count && indices[2] < view.entry_count &&
            isfinite(view.entries[indices[0] * 3]) &&
            isfinite(view.entries[indices[1] * 3]) &&
            isfinite(view.entries[indices[2] * 3]) &&
            fabs(view.entries[indices[0] * 3] - .1) < 1e-7 &&
            fabs(view.entries[indices[1] * 3] - .6) < 1e-7 &&
            fabs(view.entries[indices[2] * 3] - .1) < 1e-7;
        break;
    case 36:
        ok = 1;
        for (i = 0; i < 16 && ok; ++i) {
            if (SIXEL_FAILED(direct_init(dither, linear, format))) {
                ok = 0;
                break;
            }
            exposed = sixel_dither_get_palette(dither);
            if (exposed == NULL) { ok = 0; break; }
            exposed[0] = exposed[1] = exposed[2] = 4;
            direct_fault_hits = 0;
            direct_fail_after = (int)i;
            status = sixel_dither_prepare_standalone_palette(dither);
            direct_fail_after = -1;
            if (direct_fault_hits == 0) { break; }
            memset(&capture, 0, sizeof(capture));
            ok = SIXEL_FAILED(status) && SIXEL_FAILED(sixel_encode(
                (unsigned char *)linear, 2, 1, 0, dither, output)) &&
                capture.length == 0;
            sixel_dither_set_palette(dither, linear_bytes);
            ok = ok && direct_linear_wire(dither, output, &capture, 0, 5);
        }
        ok = ok && i > 0 && i < 16;
        break;
    case 37:
        direct_fault_hits = 0;
        direct_fail_after = 0;
        sixel_dither_set_palette(dither, linear_bytes);
        direct_fail_after = -1;
        ok = direct_fault_hits != 0 && SIXEL_FAILED(sixel_encode(
            (unsigned char *)linear, 2, 1, 0, dither, output)) &&
            capture.length == 0;
        sixel_dither_set_palette(dither, linear_bytes);
        ok = ok && direct_linear_wire(dither, output, &capture, 0, 5);
        break;
    case 39:
        exposed = sixel_dither_get_palette(dither);
        if (exposed == NULL) { break; }
        exposed[0] = exposed[1] = exposed[2] = 4;
        sixel_dither_set_pixelformat(dither, SIXEL_PIXELFORMAT_RGBFLOAT32);
        status = sixel_encode((unsigned char *)linear, 2, 1, 0, dither, output);
        sixel_dither_set_pixelformat(dither, format);
        ok = SIXEL_FAILED(status) && capture.length == 0 &&
            direct_linear_wire(dither, output, &capture, 0, 13);
        break;
    default:
        break;
    }
end:
    direct_fail_after = -1;
    if (!ok) {
        fprintf(stderr, "direct float case %d failed: %s\n", mode,
            sixel_helper_get_additional_message());
    }
    sixel_allocator_free(allocator, indices);
    sixel_output_unref(output);
    sixel_dither_unref(other);
    sixel_dither_unref(dither);
    sixel_allocator_unref(allocator);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}

#if HAVE_DIAGNOSTIC_DEPRECATED_DECLARATIONS
# pragma GCC diagnostic pop
#endif
