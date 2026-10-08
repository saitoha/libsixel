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

/* A complete parameter group must end here: a prefix such as 2 must not
 * accept 20, and an additional parameter must not hide a malformed group. */
static int
direct_definition(char const *stream, char const *expected)
{
    char const *match;
    char next;
    size_t length;

    length = strlen(expected);
    match = strstr(stream, expected);
    while (match != NULL) {
        next = match[length];
        if ((next < '0' || next > '9') && next != ';') {
            return 1;
        }
        match = strstr(match + 1, expected);
    }
    return 0;
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
            direct_definition(capture->bytes, expected) &&
            direct_definition(capture->bytes, ";1;0;51;0");
    }
    (void)snprintf(expected, sizeof(expected), ";2;%d;%d;%d",
        dark, dark, dark);
    return SIXEL_SUCCEEDED(status) &&
        direct_definition(capture->bytes, expected) &&
        direct_definition(capture->bytes, ";2;51;51;51");
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
            direct_definition(capture->bytes, expected) &&
            direct_definition(capture->bytes, ";1;0;54;0");
    }
    (void)snprintf(expected, sizeof(expected), ";2;%d;%d;%d",
        dark, dark, dark);
    return SIXEL_SUCCEEDED(status) &&
        direct_definition(capture->bytes, expected) &&
        direct_definition(capture->bytes, ";2;54;54;54");
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

/* Perceptual samples share coordinates, not physical colors. Output literals
 * are independently derived from the 60-digit inverse-matrix/scalar method
 * in tools/generate_color_fixtures.py (frame-byte fixtures). RGB101 and HLS
 * use the gamma result directly, without an intermediate RGB8 projection.
 * Sources are exact stored floats; the byte witness starts at byte/255 with
 * the documented opponent offset and scale. Minimum distance to a rounding
 * boundary is 0.016 percentage points/degrees. A 0.002-unit output budget
 * covers inverse-coefficient error, float stores and nonlinear propagation
 * for these well-conditioned samples, and cannot change any expected code. */
static int const direct_perceptual_formats[] = {
    SIXEL_PIXELFORMAT_OKLABFLOAT32, SIXEL_PIXELFORMAT_CIELABFLOAT32,
    SIXEL_PIXELFORMAT_DIN99DFLOAT32
};
static float const direct_perceptual_scale[] = {1.0f, 3.0f, 2.0f};
static unsigned char const direct_perceptual_bytes[] = {
    51, 115, 143, 166, 145, 119
};

static void
direct_perceptual_decode(float *values, unsigned char const *bytes, int space)
{
    int i;

    /* Independent compatibility equation; do not call the production
     * pixelformat converter to construct its own expected values. */
    for (i = 0; i < 6; ++i) {
        values[i] = (float)((double)bytes[i] / 255.0);
        if (i % 3 != 0) {
            values[i] = (float)(((double)bytes[i] / 255.0 - .5) *
                direct_perceptual_scale[space]);
        }
    }
}

static int
direct_perceptual_entries(sixel_dither_t *dither, float const *expected)
{
    sixel_palette_float32_entries_view_t view;
    unsigned int i;
    unsigned int j;
    unsigned int c;
    unsigned int seen;
    int match;

    memset(&view, 0, sizeof(view));
    if (!dither->use_rgb101_palette || SIXEL_FAILED(
            dither->palette->vtbl->get_entries_float32(dither->palette,
                &view)) || view.entries == NULL || view.entry_count != 2) {
        return 0;
    }
    /* Scale/unscale and final float stores: 2e-6 absolute component units
     * over [-3,3], well below any relevant compatibility byte spacing. */
    seen = 0;
    for (i = 0; i < 2; ++i) {
        for (j = 0; j < 2; ++j) {
            match = 1;
            for (c = 0; c < 3; ++c) {
                match = match && isfinite(view.entries[i * 3 + c]) &&
                    fabs(view.entries[i * 3 + c] - expected[j * 3 + c])
                        < 2e-6;
            }
            if (match) { seen |= 1U << j; }
        }
    }
    return seen == 3;
}

static int
direct_perceptual_wire(sixel_dither_t *dither, sixel_output_t *output,
                      direct_capture_t *capture, int space, int kind)
{
    static char const *const expected[3][3][2] = {
        {{";2;7;10;2", ";2;85;29;78"},
         {";1;199;6;63", ";1;68;57;66"},
         {";2;4;11;0", ";2;68;49;64"}},
        {{";2;18;19;17", ";2;76;56;71"},
         {";1;202;18;7", ";1;74;66;29"},
         {";2;12;21;4", ";2;76;56;71"}},
        {{";2;16;17;16", ";2;71;53;66"},
         {";1;212;16;5", ";1;77;62;24"},
         {";2;13;18;10", ";2;66;55;62"}}
    };
    float query[] = {.2f, -.02f, .03f, .65f, .2f, -.1f};
    SIXELSTATUS status;

    if (direct_definition(";2;7;10;20#0@", ";2;7;10;2") ||
            direct_definition(";2;7;10;2;0#0@", ";2;7;10;2")) {
        return 0;
    }
    if (kind == 2) {
        direct_perceptual_decode(query, direct_perceptual_bytes, space);
    }
    memset(capture, 0, sizeof(*capture));
    sixel_output_set_palette_type(output, kind == 1
        ? SIXEL_PALETTETYPE_HLS : SIXEL_PALETTETYPE_RGB);
    status = sixel_encode((unsigned char *)query, 2, 1, 0, dither, output);
    return SIXEL_SUCCEEDED(status) &&
        direct_definition(capture->bytes, expected[space][kind][0]) &&
        direct_definition(capture->bytes, expected[space][kind][1]);
}

#if HAVE_DIAGNOSTIC_DEPRECATED_DECLARATIONS
# pragma GCC diagnostic push
# pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif
static int
direct_perceptual(int mode)
{
    float source[] = {.2f, -.02f, .03f, .65f, .2f, -.1f};
    float extended[] = {-.1f, -1, 1, 1.2f, .0249f, -.0449f};
    float close[] = {.4f, -.01f, .03f, .4f, -.009f, .03f};
    float centers[] = {.5f, -.2f, .025f, .5f, .2f, .025f};
    float query[] = {.4f, -.0096f, .03f};
    float fs[] = {.5f, -.06f, .025f, .5f, -.06f, .025f,
                  .5f, -.06f, .025f};
    float gamma[] = {.0149f, .0149f, .0149f, .5149f, .5149f, .5149f};
    float linear[] = {.002f, .002f, .002f, .25f, .25f, .25f};
    float decoded[6];
    unsigned char bytes[] = {4, 4, 4, 131, 131, 131};
    unsigned char copy[6];
    unsigned char *exposed;
    sixel_palette_float32_entries_view_t view;
    sixel_allocator_t *allocator;
    sixel_dither_t *dither;
    sixel_dither_t *other;
    sixel_output_t *output;
    sixel_index_t *indices;
    direct_capture_t capture;
    SIXELSTATUS status;
    unsigned int i;
    int space;
    int format;
    int operation;
    int ok;

    allocator = NULL;
    dither = other = NULL;
    output = NULL;
    indices = NULL;
    ok = 0;
    space = mode < 70 ? (mode - 40) / 10 : (mode - 70) % 3;
    operation = mode < 70 ? (mode - 40) % 10 : mode;
    format = direct_perceptual_formats[space];
    memset(&capture, 0, sizeof(capture));
    memset(&view, 0, sizeof(view));
    if (SIXEL_FAILED(sixel_allocator_new(&allocator, direct_malloc,
            direct_calloc, direct_realloc, free)) ||
            SIXEL_FAILED(sixel_dither_new(&dither, 2, allocator)) ||
            SIXEL_FAILED(sixel_output_new(&output, direct_write, &capture,
                allocator)) ||
            SIXEL_FAILED(direct_init(dither, source, format))) {
        goto end;
    }
    switch (operation) {
    case 0:
        extended[1] = -direct_perceptual_scale[space];
        extended[2] = direct_perceptual_scale[space];
        ok = SIXEL_SUCCEEDED(direct_init(dither, extended, format)) &&
            direct_perceptual_entries(dither, extended);
        break;
    case 1:
    case 2:
        ok = direct_perceptual_wire(dither, output, &capture,
            space, operation - 1);
        break;
    case 3:
        if (SIXEL_FAILED(direct_init(dither, close, format))) { break; }
        indices = sixel_dither_apply_palette(dither,
            (unsigned char *)query, 1, 1);
        /* Opponent midpoint -.0095: -.0096 must select -.01. Both
         * candidates have the same compatibility byte in all three spaces.
         * The 2e-6 coordinate budget cannot cross this 1e-4 margin. */
        ok = indices != NULL && SIXEL_SUCCEEDED(
            dither->palette->vtbl->get_entries_float32(dither->palette,
                &view)) && view.entries != NULL &&
            indices[0] < view.entry_count &&
            isfinite(view.entries[indices[0] * 3 + 1]) &&
            fabs(view.entries[indices[0] * 3 + 1] + .01) < 2e-6;
        break;
    case 4:
        direct_fault_hits = 0;
        direct_fail_after = 0;
        exposed = sixel_dither_get_palette(dither);
        direct_fail_after = -1;
        ok = exposed != NULL && direct_fault_hits == 0 &&
            direct_perceptual_entries(dither, source) &&
            direct_perceptual_wire(dither, output, &capture, space, 0);
        break;
    case 5:
        exposed = sixel_dither_get_palette(dither);
        if (exposed == NULL) { break; }
        memcpy(exposed, direct_perceptual_bytes, sizeof(copy));
        direct_perceptual_decode(decoded, direct_perceptual_bytes, space);
        ok = direct_perceptual_wire(dither, output, &capture, space, 2) &&
            direct_perceptual_entries(dither, decoded);
        break;
    case 6:
        exposed = sixel_dither_get_palette(dither);
        if (exposed == NULL) { break; }
        memcpy(copy, exposed, sizeof(copy));
        direct_perceptual_decode(decoded, copy, space);
        sixel_dither_set_palette(dither, copy);
        ok = direct_perceptual_entries(dither, decoded);
        break;
    case 7:
        if (SIXEL_FAILED(direct_init(dither, centers, format)) ||
                SIXEL_FAILED(sixel_dither_set_clamp_policy(dither,
                    SIXEL_DITHER_CLAMP_NONE, 1.01))) { break; }
        sixel_dither_set_diffusion_type(dither, SIXEL_DIFFUSE_FS);
        indices = sixel_dither_apply_palette(dither,
            (unsigned char *)fs, 3, 1);
        /* Opponent-axis FS: -.06 -> -.2; -.06+.14*7/16=.00125
         * -> .2; -.06+(.00125-.2)*7/16=-.146953125 -> -.2.
         * L/b residuals are zero. The smallest margin is .00125. */
        if (indices == NULL || SIXEL_FAILED(
                dither->palette->vtbl->get_entries_float32(dither->palette,
                    &view)) || view.entries == NULL) { break; }
        ok = 1;
        for (i = 0; i < 3; ++i) {
            ok = ok && indices[i] < view.entry_count &&
                isfinite(view.entries[indices[i] * 3 + 1]) &&
                fabs(view.entries[indices[i] * 3 + 1] -
                    (i == 1 ? .2 : -.2)) < 2e-6;
        }
        break;
    case 8:
        exposed = sixel_dither_get_palette(dither);
        if (exposed == NULL) { break; }
        memcpy(exposed, direct_perceptual_bytes, sizeof(copy));
        sixel_dither_set_pixelformat(dither,
            direct_perceptual_formats[(space + 1) % 3]);
        status = sixel_encode((unsigned char *)source, 2, 1, 0,
            dither, output);
        sixel_dither_set_pixelformat(dither, format);
        ok = SIXEL_FAILED(status) && capture.length == 0 &&
            direct_perceptual_wire(dither, output, &capture, space, 2);
        break;
    case 9:
        source[1] = NAN;
        status = direct_init(dither, source, format);
        source[1] = -.02f;
        ok = SIXEL_FAILED(status) && SIXEL_FAILED(sixel_encode(
            (unsigned char *)source, 2, 1, 0, dither, output)) &&
            capture.length == 0 &&
            SIXEL_SUCCEEDED(direct_init(dither, source, format)) &&
            direct_perceptual_wire(dither, output, &capture, space, 0);
        break;
    case 70:
        ok = 1;
        for (i = 0; i < 3 && ok; ++i) {
            ok = SIXEL_SUCCEEDED(direct_init(dither, source,
                    direct_perceptual_formats[i])) &&
                direct_perceptual_wire(dither, output, &capture, (int)i, 0);
        }
        ok = ok && SIXEL_SUCCEEDED(direct_init(dither, gamma,
            SIXEL_PIXELFORMAT_RGBFLOAT32)) &&
            direct_wire(dither, output, &capture, 0, 1) &&
            SIXEL_SUCCEEDED(direct_init(dither, linear,
                SIXEL_PIXELFORMAT_LINEARRGBFLOAT32)) &&
            direct_linear_wire(dither, output, &capture, 0, 3) &&
            SIXEL_SUCCEEDED(direct_init(dither, bytes,
                SIXEL_PIXELFORMAT_RGB888)) &&
            direct_wire(dither, output, &capture, 0, 2) &&
            SIXEL_SUCCEEDED(direct_init(dither, source, format)) &&
            direct_perceptual_wire(dither, output, &capture, space, 0);
        break;
    case 71:
        ok = SIXEL_SUCCEEDED(sixel_dither_new(&other, 2, allocator)) &&
            SIXEL_SUCCEEDED(direct_init(other, source,
                direct_perceptual_formats[2])) &&
            direct_perceptual_wire(dither, output, &capture, space, 0) &&
            direct_perceptual_wire(other, output, &capture, 2, 1) &&
            direct_perceptual_wire(dither, output, &capture, space, 1) &&
            direct_perceptual_wire(other, output, &capture, 2, 0);
        break;
    case 72:
        ok = 1;
        for (i = 0; i < 128 && ok; ++i) {
            direct_fault_hits = 0;
            direct_fail_after = (int)i;
            status = direct_init(dither, source, format);
            direct_fail_after = -1;
            if (direct_fault_hits == 0) { break; }
            memset(&capture, 0, sizeof(capture));
            ok = SIXEL_FAILED(status) && SIXEL_FAILED(sixel_encode(
                (unsigned char *)source, 2, 1, 0, dither, output)) &&
                capture.length == 0 &&
                SIXEL_SUCCEEDED(direct_init(dither, source, format)) &&
                direct_perceptual_wire(dither, output, &capture, space, 0);
        }
        ok = ok && i > 0 && i < 128;
        break;
    case 73:
        direct_fault_hits = 0;
        direct_fail_after = 0;
        sixel_dither_set_palette(dither,
            (unsigned char *)direct_perceptual_bytes);
        direct_fail_after = -1;
        ok = direct_fault_hits != 0 && SIXEL_FAILED(sixel_encode(
            (unsigned char *)source, 2, 1, 0, dither, output)) &&
            capture.length == 0;
        sixel_dither_set_palette(dither,
            (unsigned char *)direct_perceptual_bytes);
        ok = ok && direct_perceptual_wire(dither, output, &capture, space, 2);
        break;
    case 74:
        ok = 1;
        for (i = 0; i < 16 && ok; ++i) {
            if (SIXEL_FAILED(direct_init(dither, source, format))) {
                ok = 0;
                break;
            }
            exposed = sixel_dither_get_palette(dither);
            if (exposed == NULL) { ok = 0; break; }
            memcpy(exposed, direct_perceptual_bytes, sizeof(copy));
            direct_fault_hits = 0;
            direct_fail_after = (int)i;
            status = sixel_dither_prepare_standalone_palette(dither);
            direct_fail_after = -1;
            if (direct_fault_hits == 0) { break; }
            memset(&capture, 0, sizeof(capture));
            ok = SIXEL_FAILED(status) && SIXEL_FAILED(sixel_encode(
                (unsigned char *)source, 2, 1, 0, dither, output)) &&
                capture.length == 0;
            sixel_dither_set_palette(dither,
                (unsigned char *)direct_perceptual_bytes);
            ok = ok && direct_perceptual_wire(dither, output,
                &capture, space, 2);
        }
        ok = ok && i > 0 && i < 16;
        break;
    case 75:
        /* Finite DIN99d L=3000 makes expm1(100*L/325.22) overflow.
         * Construction must not publish an unusable output cache. */
        source[0] = 3000;
        status = direct_init(dither, source, format);
        source[0] = .2f;
        ok = SIXEL_FAILED(status) && SIXEL_FAILED(sixel_encode(
            (unsigned char *)source, 2, 1, 0, dither, output)) &&
            capture.length == 0 &&
            SIXEL_SUCCEEDED(direct_init(dither, source, format)) &&
            direct_perceptual_wire(dither, output, &capture, space, 0);
        break;
    default:
        break;
    }
end:
    direct_fail_after = -1;
    if (!ok) {
        fprintf(stderr, "direct perceptual case %d failed: %s\n", mode,
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
    if (mode >= 40 && mode <= 75) {
        return direct_perceptual(mode);
    }
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
