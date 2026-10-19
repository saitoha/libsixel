/* SPDX-License-Identifier: MIT */
/* Independent palette values and index oracles for native OR output. */
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
#include "src/planner.h"
#include "src/encoder-core-private.h"

typedef struct or_native_capture {
    char bytes[16384];
    size_t length;
} or_native_capture_t;

static int
or_native_write(char *data, int length, void *context)
{
    or_native_capture_t *capture;

    capture = context;
    if (length < 0 || (size_t)length >=
            sizeof(capture->bytes) - capture->length) {
        return -1;
    }
    memcpy(capture->bytes + capture->length, data, (size_t)length);
    capture->length += (size_t)length;
    capture->bytes[capture->length] = '\0';
    return length;
}

static int
or_native_definition(or_native_capture_t const *capture, int index,
                     int r, int g, int b)
{
    char expected[64];
    char const *match;

    snprintf(expected, sizeof(expected), "#%d;2;%d;%d;%d", index, r, g, b);
    match = strstr(capture->bytes, expected);
    /* A following decimal digit would make this only a prefix match. */
    return match != NULL && (match[strlen(expected)] < '0' ||
        match[strlen(expected)] > '9');
}

static int
or_native_indices(or_native_capture_t const *capture, int width, int height,
                  int const *expected)
{
    sixel_allocator_t *allocator;
    unsigned char *pixels;
    unsigned char *palette;
    int w;
    int h;
    int colors;
    int n;
    int ok;

    allocator = NULL;
    pixels = NULL;
    palette = NULL;
    ok = 0;
    if (strncmp(capture->bytes, "\033P7;5q", 6) != 0 ||
            SIXEL_FAILED(sixel_allocator_new(&allocator, NULL, NULL,
                NULL, NULL)) ||
            SIXEL_FAILED(sixel_decode_raw((unsigned char *)capture->bytes,
                (int)capture->length, &pixels, &w, &h, &palette, &colors,
                allocator)) || w != width || h != height) {
        goto end;
    }
    for (n = 0; n < w * h; ++n) {
        if (pixels[n] != expected[n]) {
            fprintf(stderr, "OR pixel %d = %u, expected %d\n", n,
                pixels[n], expected[n]);
            goto end;
        }
    }
    ok = 1;
end:
    if (allocator != NULL) {
        sixel_allocator_free(allocator, pixels);
        sixel_allocator_free(allocator, palette);
    }
    sixel_allocator_unref(allocator);
    return ok;
}

int
or_native_encoder_case(int scenario)
{
    static struct { int flag; char const *value; } const options[] = {
        { '=', "1" }, { 'O', NULL },
        { 'Q', "heckbert" }, { 'F', "none" }, { '_', "none" },
        { '~', "none" }, { 'd', "none" }, { 'E', "size" },
        { 'X', "linear" }, { 'W', "linear" }, { '7', NULL },
        { '4', "full-frame" }, { '5', "none" }
    };
    static float const gray[3] = { .1f, .1001f, .5f };
    static float const fixed[4] = { 0.f, .0908417112f, .40197778f, 1.f };
    static int const gray_wire[3] = { 35, 35, 74 };
    float pixels[5 * 13 * 3];
    float saved[5 * 13 * 3];
    unsigned char bytes[5 * 13 * 3];
    int expected[7 * 20];
    int mapping[3];
    sixel_encoder_t *encoder;
    sixel_frame_t *frame;
    sixel_output_t *output;
    sixel_dither_t *dither;
    sixel_palette_float32_entries_view_t view;
    sixel_frame_transparency_t transparency;
    sixel_frame_interface_t *iface;
    unsigned char *mask;
    or_native_capture_t capture;
    SIXELSTATUS status;
    size_t n;
    unsigned int i;
    int x;
    int y;
    int c;
    int width;
    int height;
    int colors;
    int p3;
    int centroid;
    int byte;
    int builtin;
    int offset;
    int matched;
    int ok;
    double target;

    encoder = NULL;
    frame = NULL;
    output = NULL;
    mask = NULL;
    ok = 0;
    centroid = scenario < 2 || scenario == 8;
    byte = scenario == 7;
    builtin = scenario == 6;
    offset = scenario == 5;
    p3 = scenario != 0 && scenario != 2 && !byte;
    width = centroid ? 2 : 5;
    height = centroid ? 1 : 13;
    colors = centroid ? 1 : (builtin || byte) ? 4 : 3;
    memset(&capture, 0, sizeof(capture));
    for (n = 0; n < sizeof(pixels) / sizeof(pixels[0]); ++n) {
        c = (int)(n / 3) % 3;
        pixels[n] = builtin ? (c == 0 ? .1f : c == 1 ? .4f : .8f) : gray[c];
        bytes[n] = (unsigned char)(c * 127);
    }
    if (centroid) {
        pixels[0] = 2.f;
        pixels[3] = -.25f;
        pixels[1] = pixels[2] = pixels[4] = pixels[5] = .1f;
    }
    memcpy(saved, pixels, sizeof(saved));
    if (SIXEL_FAILED(sixel_encoder_new(&encoder, NULL)) ||
            SIXEL_FAILED(sixel_output_new(&output, or_native_write,
                &capture, NULL))) {
        goto end;
    }
    for (n = 0; n < sizeof(options) / sizeof(options[0]); ++n) {
        if (SIXEL_FAILED(sixel_encoder_setopt(encoder, options[n].flag,
                options[n].value))) {
            fprintf(stderr, "OR option %c rejected\n", options[n].flag);
            goto end;
        }
    }
    if (SIXEL_FAILED(sixel_encoder_setopt(encoder, 'U',
            p3 ? "display-p3" : "gamma"))) {
        goto end;
    }
    if (!builtin && !byte && scenario != 9 &&
            SIXEL_FAILED(sixel_encoder_setopt(encoder, 'p',
                centroid ? "1" : "3"))) {
        goto end;
    }
    if ((builtin || byte) && SIXEL_FAILED(sixel_encoder_setopt(encoder,
            'b', "gray2"))) {
        goto end;
    }
    if (byte && (SIXEL_FAILED(sixel_encoder_setopt(encoder, 'X', "gamma")) ||
            SIXEL_FAILED(sixel_encoder_setopt(encoder, 'W', "gamma")))) {
        goto end;
    }
    if ((scenario == 4 || offset) &&
            SIXEL_FAILED(sixel_encoder_setopt(encoder, '=', "4"))) {
        goto end;
    }
    if (offset && SIXEL_FAILED(sixel_encoder_setopt(encoder, '+', "2,7"))) {
        goto end;
    }
    if (scenario == 9 && SIXEL_FAILED(sixel_encoder_setopt(encoder,
            'e', NULL))) {
        goto end;
    }
    if (SIXEL_FAILED(sixel_frame_new(&frame, NULL)) ||
            SIXEL_FAILED(sixel_frame_init_borrowed(frame,
                byte ? (void *)bytes : (void *)pixels, width, height,
                byte ? SIXEL_PIXELFORMAT_RGB888 :
                SIXEL_PIXELFORMAT_LINEARRGBFLOAT32, NULL, -1))) {
        goto end;
    }
    if (scenario == 8) {
        if (SIXEL_FAILED(sixel_encoder_setopt(encoder, 'p', "2"))) {
            goto end;
        }
        mask = calloc(2, 1);
        if (mask == NULL) {
            goto end;
        }
        mask[1] = 1;
        memset(&transparency, 0, sizeof(transparency));
        transparency.transparent = -1;
        transparency.alpha_zero_is_transparent = 1;
        transparency.transparent_mask = mask;
        transparency.transparent_mask_size = 2;
        iface = sixel_frame_as_interface(frame);
        if (SIXEL_FAILED(iface->vtbl->set_transparency(iface,
                &transparency))) {
            goto end;
        }
        mask = NULL;
        status = sixel_encoder_encode_frame(encoder, frame, output);
        if (status != SIXEL_BAD_INPUT || capture.length != 0) {
            goto end;
        }
        /* Two slots make the mask valid for ordinary P3; OR alone rejects
         * it. Restore the one-center budget for the later opaque encode. */
        if (SIXEL_FAILED(sixel_encoder_setopt(encoder, 'p', "1"))) {
            goto end;
        }
        sixel_frame_unref(frame);
        frame = NULL;
        if (SIXEL_FAILED(sixel_frame_new(&frame, NULL)) ||
                SIXEL_FAILED(sixel_frame_init_borrowed(frame, pixels, 2, 1,
                    SIXEL_PIXELFORMAT_LINEARRGBFLOAT32, NULL, -1))) {
            goto end;
        }
    }
    status = sixel_encoder_encode_frame(encoder, frame, output);
    if (scenario == 9) {
        ok = status == SIXEL_BAD_ARGUMENT && capture.length == 0;
        goto end;
    }
    if (SIXEL_FAILED(status)) {
        fprintf(stderr, "native OR status %d: %s\n", status,
            sixel_helper_get_additional_message());
        goto end;
    }
    dither = (sixel_dither_t *)encoder->diagnostic_dither;
    if (dither == NULL || dither->use_rgb101_palette != !byte ||
            memcmp(pixels, saved, sizeof(saved)) != 0 ||
            dither->ncolors != colors) {
        goto end;
    }
    if (scenario == 4 && dither->pipeline_last_encode_threads < 2) {
        fprintf(stderr, "OR pipeline did not run\n");
        goto end;
    }
    /* The resolved mode identifies the direct offset path. Encode capacity
     * may still report the configured budget without a row pipeline. */
    if (offset &&
            (dither->pipeline_last_mode != SIXEL_DITHER_ENCODE_SERIAL ||
             dither->pipeline_last_band_height != 0)) {
        goto end;
    }
    if (!byte) {
        if (SIXEL_FAILED(dither->palette->vtbl->get_entries_float32(
                dither->palette, &view)) || view.entries == NULL ||
                view.entry_count != (unsigned int)colors) {
            goto end;
        }
        /* Match independent source/centroid labels without prescribing the
         * quantizer's palette order. Distinct sub-byte centers may have the
         * same RGB101 code, but their lookup indices must remain distinct. */
        for (c = 0; c < colors; ++c) {
            matched = 0;
            for (i = 0; i < view.entry_count; ++i) {
                for (n = 0; n < 3; ++n) {
                    target = centroid ? (n == 0 ? .875 : .1) :
                        builtin ? fixed[c] : gray[c];
                    if (!isfinite(view.entries[i * 3 + n]) ||
                            fabs(view.entries[i * 3 + n] - target) > 5e-7) {
                        break;
                    }
                }
                if (n == 3) {
                    matched = 1;
                    break;
                }
            }
            if (!matched) {
                goto end;
            }
            if (c < 3) {
                mapping[c] = (int)i;
            }
            if (centroid) {
                if (!or_native_definition(&capture, (int)i,
                        p3 ? 87 : 94, p3 ? 39 : 35, p3 ? 37 : 35)) {
                    goto end;
                }
            } else {
                x = builtin ? (c * 100 + 1) / 3 : gray_wire[c];
                if (!or_native_definition(&capture, (int)i, x, x, x)) {
                    goto end;
                }
            }
        }
    } else {
        mapping[0] = 0;
        mapping[1] = 1;
        mapping[2] = 3;
        for (c = 0; c < 4; ++c) {
            x = (c * 100 + 1) / 3;
            if (!or_native_definition(&capture, c, x, x, x)) {
                goto end;
            }
        }
    }
    for (y = 0; y < height + (offset ? 7 : 0); ++y) {
        for (x = 0; x < width + (offset ? 2 : 0); ++x) {
            c = ((y - (offset ? 7 : 0)) * width +
                x - (offset ? 2 : 0)) % 3;
            expected[y * (width + (offset ? 2 : 0)) + x] =
                offset && (x < 2 || y < 7) ? 0 :
                centroid ? mapping[0] : builtin ? c + 1 : mapping[c];
        }
    }
    ok = or_native_indices(&capture, width + (offset ? 2 : 0),
        height + (offset ? 7 : 0), expected);
end:
    if (!ok) {
        fprintf(stderr, "native OR case %d: %s\n", scenario,
            sixel_helper_get_additional_message());
    }
    free(mask);
    sixel_frame_unref(frame);
    sixel_output_unref(output);
    sixel_encoder_unref(encoder);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}

int
or_native_writer_case(int pipeline)
{
    sixel_output_t *output;
    sixel_dither_t *dither;
    or_native_capture_t capture;
    unsigned char bytes[101 * 3];
    float wire[101 * 3];
    sixel_index_t indices[101];
    unsigned char pixels[2 * 13 * 3];
    int expected[101];
    int n;
    int colors;
    int width;
    int height;
    int ok;
    SIXELSTATUS status;

    output = NULL;
    dither = NULL;
    ok = 0;
    memset(&capture, 0, sizeof(capture));
    memset(bytes, 0, sizeof(bytes));
    colors = pipeline ? 2 : 101;
    width = pipeline ? 2 : 101;
    height = pipeline ? 13 : 1;
    for (n = 0; n < 101; ++n) {
        indices[n] = (sixel_index_t)n;
        wire[n * 3] = (float)n / 100.f;
        wire[n * 3 + 1] = (float)(100 - n) / 100.f;
        wire[n * 3 + 2] = (float)((n * 3) % 101) / 100.f;
        expected[n] = pipeline ? n % 2 : n;
    }
    for (n = 0; n < 2 * 13 * 3; ++n) {
        pixels[n] = n / 3 % 2 ? 255 : 0;
    }
    if (SIXEL_FAILED(sixel_output_new(&output, or_native_write,
            &capture, NULL))) {
        goto end;
    }
    sixel_output_set_ormode(output, 1);
    /* This private body test supplies its own known envelope. Include both
     * flushed packets and the pending output buffer in the captured body. */
    capture.length = (size_t)snprintf(capture.bytes, sizeof(capture.bytes),
        "\033P7;5q\"1;1;%d;%d", width, height);
    if (pipeline) {
        if (SIXEL_FAILED(sixel_dither_new(&dither, 2, NULL))) {
            goto end;
        }
        bytes[3] = bytes[4] = bytes[5] = 255;
        sixel_dither_set_palette(dither, bytes);
        sixel_dither_set_pixelformat(dither, SIXEL_PIXELFORMAT_RGB888);
        sixel_dither_set_diffusion_type(dither, SIXEL_DIFFUSE_NONE);
        sixel_dither_set_optimize_palette(dither, 0);
        /* The supplied wire view deliberately differs from this lookup's
         * black/white compatibility palette, exposing a lost float view. */
        status = sixel_encode_body_ormode_pipeline(pixels, width, height,
            bytes, wire, dither, output, 2);
    } else {
        status = sixel_encode_body_ormode(indices, width, height, bytes,
            wire, colors, -1, output);
    }
    if (SIXEL_FAILED(status) ||
            or_native_write((char *)output->buffer, output->pos,
                &capture) < 0 ||
            or_native_write("\033\\", 2, &capture) < 0) {
        goto end;
    }
    for (n = 0; n < colors; ++n) {
        if (!or_native_definition(&capture, n,
                n, 100 - n, (n * 3) % 101)) {
            goto end;
        }
    }
    ok = or_native_indices(&capture, width, height, expected);
end:
    sixel_output_unref(output);
    sixel_dither_unref(dither);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
