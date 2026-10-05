/* SPDX-License-Identifier: MIT */
/* Independent working values and DEC HLS fields at the output boundary. */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sixel.h>
#include "src/encoder.h"
#include "src/dither.h"
#include "src/palette.h"
#include "src/sixel_decode_pixels.h"

typedef struct hls_capture {
    char bytes[32768];
    size_t size;
} hls_capture_t;

static int
hls_write(char *data, int size, void *context)
{
    hls_capture_t *capture;

    capture = context;
    if (size < 0 || (size_t)size >= sizeof(capture->bytes) - capture->size) {
        return -1;
    }
    memcpy(capture->bytes + capture->size, data, (size_t)size);
    capture->size += (size_t)size;
    capture->bytes[capture->size] = '\0';
    return size;
}

static int
hls_definition(hls_capture_t const *capture, int index, int type,
               int a, int b, int c)
{
    char text[64];
    char const *found;

    snprintf(text, sizeof(text), "#%d;%d;%d;%d;%d", index, type, a, b, c);
    found = strstr(capture->bytes, text);
    return found != NULL && (found[strlen(text)] < '0' ||
        found[strlen(text)] > '9');
}

static int
hls_indices(hls_capture_t *capture, int width, int height,
            int const *expected, int masked)
{
    sixel_allocator_t *allocator;
    unsigned char *pixels;
    unsigned char *paint;
    unsigned char *palette;
    unsigned int flags;
    int w;
    int h;
    int colors;
    int n;
    int ok;

    allocator = NULL;
    pixels = paint = palette = NULL;
    ok = 0;
    if (SIXEL_FAILED(sixel_allocator_new(&allocator, NULL, NULL,
                NULL, NULL)) ||
            SIXEL_FAILED(sixel_decode_raw_with_options_mask(
                (unsigned char *)capture->bytes, (int)capture->size, 0,
                &pixels, &paint, &w, &h, &palette, &colors, &flags,
                allocator)) || w != width || h != height || paint == NULL) {
        goto end;
    }
    for (n = 0; n < w * h; ++n) {
        if (n == masked ? paint[n] != 0 :
                paint[n] == 0 || pixels[n] != expected[n]) {
            fprintf(stderr, "HLS pixel %d: index=%u paint=%u expected=%d\n",
                n, pixels[n], paint[n], expected[n]);
            goto end;
        }
    }
    ok = 1;
end:
    if (allocator != NULL) {
        sixel_allocator_free(allocator, pixels);
        sixel_allocator_free(allocator, paint);
        sixel_allocator_free(allocator, palette);
    }
    sixel_allocator_unref(allocator);
    return ok;
}

static int
hls_native_run(int scenario, int snapshot)
{
    static struct { int flag; char const *value; } const options[] = {
        { '=', "1" }, { 'Q', "heckbert" }, { 'F', "none" },
        { '_', "none" }, { '~', "none" }, { 'd', "none" },
        { 'E', "size" }, { 'X', "gamma" }, { 'W', "gamma" },
        { '7', NULL }, { 'g', NULL }, { 't', "hls" },
        { '4', "full-frame" }, { '5', "none" }
    };
    static float const rounding[3] = { .123f, .456f, .789f };
    static float const hues[9][3] = {
        { 1, 0, 0 }, { 0, 1, 0 }, { 0, 0, 1 },
        { 1, 1, 0 }, { 0, 1, 1 }, { 1, 0, 1 },
        { .234f, .234f, .234f }, { 2, 2, 2 }, { -.25f, -.25f, -.25f }
    };
    static int const fields[9][3] = {
        { 120, 50, 100 }, { 240, 50, 100 }, { 0, 50, 100 },
        { 180, 50, 100 }, { 300, 50, 100 }, { 60, 50, 100 },
        { 0, 23, 0 }, { 0, 100, 0 }, { 0, 0, 0 }
    };
    float pixels[5 * 13 * 3];
    float saved[5 * 13 * 3];
    unsigned char bytes[5 * 13 * 3];
    unsigned char *mask;
    int mapping[9];
    int expected[5 * 13];
    sixel_encoder_t *encoder;
    sixel_frame_t *frame;
    sixel_output_t *output;
    sixel_dither_t *dither;
    sixel_palette_float32_entries_view_t view;
    sixel_frame_transparency_t transparency;
    hls_capture_t capture;
    double target;
    int centroid;
    int colorful;
    int builtin;
    int byte;
    int p3;
    int width;
    int height;
    int count;
    int i;
    int j;
    int c;
    int n;
    int step;
    int steps;
    int match;
    int ok;

    encoder = NULL;
    frame = NULL;
    output = NULL;
    mask = NULL;
    ok = 0;
    centroid = scenario < 2 || scenario == 7 || scenario == 10;
    colorful = scenario == 4 || scenario == 8 || scenario == 9 ||
        scenario == 12;
    builtin = scenario == 5 || scenario == 6 || scenario == 11;
    byte = scenario == 6 || scenario == 11;
    width = centroid ? (scenario == 7 ? 3 : 2) : 5;
    height = centroid ? 1 : 13;
    count = colorful ? 9 : builtin ? 4 : 1;
    steps = scenario == 10 ? 3 : 1;
    for (i = 0; i < 5 * 13; ++i) {
        for (c = 0; c < 3; ++c) {
            pixels[i * 3 + c] = colorful ? hues[i % 9][c] :
                builtin ? (i % 3 == 0 ? .1f : i % 3 == 1 ? .4f : .8f) :
                rounding[c];
            bytes[i * 3 + c] = (unsigned char)((i % 3) * 127);
        }
    }
    if (centroid) {
        pixels[0] = 2.f;
        pixels[3] = -.25f;
        pixels[1] = pixels[2] = pixels[4] = pixels[5] = .1f;
    }
    memcpy(saved, pixels, sizeof(saved));
    if (SIXEL_FAILED(sixel_encoder_new(&encoder, NULL)) ||
            SIXEL_FAILED(sixel_output_new(&output, hls_write,
                &capture, NULL))) {
        goto end;
    }
    if (snapshot && SIXEL_FAILED(sixel_encoder_setopt(encoder,
            'o', "png:-"))) {
        goto end;
    }
    for (n = 0; n < (int)(sizeof(options) / sizeof(options[0])); ++n) {
        if (SIXEL_FAILED(sixel_encoder_setopt(encoder,
                options[n].flag, options[n].value))) {
            goto end;
        }
    }
    if ((builtin && SIXEL_FAILED(sixel_encoder_setopt(encoder,
                'b', "gray2"))) ||
            (!builtin && SIXEL_FAILED(sixel_encoder_setopt(encoder, 'p',
                colorful ? "9" : scenario == 7 ? "2" : "1"))) ||
            (centroid && (SIXEL_FAILED(sixel_encoder_setopt(encoder,
                'X', "linear")) || SIXEL_FAILED(sixel_encoder_setopt(
                    encoder, 'W', "linear")))) ||
            ((scenario == 8 || scenario == 9) && SIXEL_FAILED(
                sixel_encoder_setopt(encoder, '=', "4"))) ||
            ((scenario == 9 || scenario == 11 || scenario == 12) &&
             SIXEL_FAILED(sixel_encoder_setopt(encoder,
                'O', NULL)))) {
        goto end;
    }
    for (step = 0; step < steps; ++step) {
        p3 = scenario == 1 || scenario == 3 || scenario == 7 ||
            (scenario == 10 && step != 1);
        memset(&capture, 0, sizeof(capture));
        if (SIXEL_FAILED(sixel_encoder_setopt(encoder, 'U',
                p3 ? "display-p3" : "gamma")) ||
                (scenario == 10 && SIXEL_FAILED(sixel_encoder_setopt(
                    encoder, 't', step == 1 ? "rgb" : "hls"))) ||
                SIXEL_FAILED(sixel_frame_new(&frame, NULL)) ||
                SIXEL_FAILED(sixel_frame_init_borrowed(frame,
                    byte ? (void *)bytes : (void *)pixels, width, height,
                    byte ? SIXEL_PIXELFORMAT_RGB888 : centroid ?
                    SIXEL_PIXELFORMAT_LINEARRGBFLOAT32 :
                    SIXEL_PIXELFORMAT_RGBFLOAT32, NULL, -1))) {
            goto end;
        }
        if (scenario == 7) {
            mask = calloc(3, 1);
            if (mask == NULL) {
                goto end;
            }
            mask[2] = 1;
            memset(&transparency, 0, sizeof(transparency));
            transparency.transparent = -1;
            transparency.alpha_zero_is_transparent = 1;
            transparency.transparent_mask = mask;
            transparency.transparent_mask_size = 3;
            if (SIXEL_FAILED(sixel_frame_as_interface(frame)->vtbl
                    ->set_transparency(sixel_frame_as_interface(frame),
                        &transparency))) {
                goto end;
            }
            mask = NULL;
        }
        if (SIXEL_FAILED(sixel_encoder_encode_frame(encoder, frame, output))) {
            goto end;
        }
        dither = (sixel_dither_t *)encoder->diagnostic_dither;
        if (dither == NULL || dither->use_rgb101_palette != !byte ||
                memcmp(pixels, saved, sizeof(saved)) != 0 ||
                dither->ncolors != count + (scenario == 7) ||
                (byte && dither->pixelformat != SIXEL_PIXELFORMAT_RGB888) ||
                ((scenario == 8 || scenario == 9) &&
                 dither->pipeline_last_encode_threads < 2)) {
            goto end;
        }
        if (!byte && (SIXEL_FAILED(dither->palette->vtbl->get_entries_float32(
                dither->palette, &view)) || view.entries == NULL)) {
            goto end;
        }
        for (j = 0; j < count; ++j) {
            match = 0;
            for (i = 0; i < count; ++i) {
                for (c = 0; c < 3 && !byte; ++c) {
                    target = centroid ? (c == 0 ? .875 : .1) :
                        colorful ? hues[j][c] : builtin ? j / 3.0 :
                        rounding[c];
                    if (!isfinite(view.entries[i * 3 + c]) ||
                            fabs(view.entries[i * 3 + c] - target) > 5e-7) {
                        break;
                    }
                }
                if (byte ? i == j : c == 3) {
                    match = 1;
                    break;
                }
            }
            if (!match) {
                goto end;
            }
            mapping[j] = i;
            /* Independent decimal matrix/transfer equations give these
             * fields, with >.0999 units to any rounding tie. RGB101 first
             * would give saturation 83/66 instead of 84/67 below, and 74
             * instead of 73 for the gamma rounding witness. */
            if (scenario == 11) {
                c = (j * 100 + 1) / 3;
                match = hls_definition(&capture, i, 2, c, c, c);
            } else if (scenario == 10 && step == 1) {
                match = hls_definition(&capture, i, 2, 94, 35, 35);
            } else if (centroid) {
                match = hls_definition(&capture, i, 1,
                    p3 ? 122 : 120, p3 ? 62 : 65, p3 ? 67 : 84);
            } else if (colorful) {
                match = hls_definition(&capture, i, 1,
                    fields[j][0], fields[j][1], fields[j][2]);
            } else if (builtin) {
                match = hls_definition(&capture, i, 1,
                    0, (j * 100 + 1) / 3, 0);
            } else {
                match = hls_definition(&capture, i, 1,
                    p3 ? 335 : 330, p3 ? 50 : 46, p3 ? 54 : 73);
            }
            if (!match) {
                fprintf(stderr, "missing HLS entry %d: %s\n", i,
                    capture.bytes);
                goto end;
            }
        }
        for (n = 0; n < width * height; ++n) {
            expected[n] = mapping[colorful ? n % 9 : builtin ?
                (byte && n % 3 == 2 ? 3 : n % 3) : 0];
        }
        if (!hls_indices(&capture, width, height, expected,
                scenario == 7 ? 2 : -1)) {
            goto end;
        }
        sixel_frame_unref(frame);
        frame = NULL;
    }
    ok = 1;
end:
    if (!ok) {
        fprintf(stderr, "native HLS case %d: %s\n", scenario,
            sixel_helper_get_additional_message());
    }
    free(mask);
    sixel_frame_unref(frame);
    sixel_output_unref(output);
    sixel_encoder_unref(encoder);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}

/* Destination selection is the only difference from the ordinary oracle.
 * encode_frame captures SIXEL here; CLI tests own actual PNG readback. */
int
hls_native_contract(int scenario)
{
    return hls_native_run(scenario, 0);
}

int
hls_png_contract(int scenario)
{
    return hls_native_run(scenario, 1);
}

/* The output views share a publication/invalidation boundary. Distinct
 * palette instances must never borrow another encoder's destination state. */
int
hls_cache_contract(int scenario)
{
    sixel_dither_t *dithers[2] = { NULL, NULL };
    sixel_palette_float32_entries_request_t request;
    float source[3] = { .875f, .1f, .1f };
    float saved[3];
    unsigned char bytes[3] = { 223, 26, 26 };
    float const *rgb;
    unsigned char const *codes;
    unsigned int count;
    int n;
    int ok;

    ok = 0;
    request.entries = source;
    request.colors = 1;
    request.depth = 3 * sizeof(float);
    for (n = 0; n < 2; ++n) {
        if (SIXEL_FAILED(sixel_dither_new(&dithers[n], 1, NULL))) {
            goto end;
        }
        sixel_dither_set_palette(dithers[n], bytes);
        if (SIXEL_FAILED(dithers[n]->palette->vtbl->init_entries_float32(
                dithers[n]->palette, &request)) ||
                SIXEL_FAILED(sixel_palette_prepare_rgb101(dithers[n]->palette,
                    SIXEL_COLORSPACE_LINEAR, n)) ||
                SIXEL_FAILED(sixel_palette_get_output_rgb(dithers[n]->palette,
                    &rgb, &count)) || count != 1) {
            goto end;
        }
        if (n == 0) {
            memcpy(saved, rgb, sizeof(saved));
        }
    }
    if (scenario == 0) {
        if (SIXEL_FAILED(sixel_palette_get_output_rgb(dithers[0]->palette,
                &rgb, &count)) || memcmp(saved, rgb, sizeof(saved)) != 0 ||
                !isfinite(rgb[0]) ||
                fabs(rgb[0] - .942904884492641) > 5e-7 ||
                SIXEL_FAILED(sixel_palette_get_output_rgb(dithers[1]->palette,
                    &rgb, &count)) ||
                !isfinite(rgb[0]) ||
                fabs(rgb[0] - .874246073590096) > 5e-7) {
            goto end;
        }
    } else {
        if (SIXEL_FAILED(dithers[0]->palette->vtbl->init_entries_float32(
                dithers[0]->palette, &request)) ||
                sixel_palette_get_output_rgb(dithers[0]->palette,
                    &rgb, &count) != SIXEL_LOGIC_ERROR || rgb != NULL ||
                count != 0 || sixel_palette_get_rgb101(dithers[0]->palette,
                    &codes, &count) != SIXEL_LOGIC_ERROR || codes != NULL ||
                SIXEL_FAILED(sixel_palette_prepare_rgb101(dithers[0]->palette,
                    SIXEL_COLORSPACE_LINEAR, 0)) ||
                SIXEL_FAILED(sixel_palette_get_output_rgb(dithers[0]->palette,
                    &rgb, &count)) || memcmp(saved, rgb, sizeof(saved)) != 0) {
            goto end;
        }
    }
    ok = 1;
end:
    sixel_dither_unref(dithers[0]);
    sixel_dither_unref(dithers[1]);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
