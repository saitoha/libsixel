/* SPDX-License-Identifier: MIT */
/* Frame-local spatial residuals and palette-cache ownership. */
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
#include "compat_stub.h"

typedef struct af_capture {
    char data[8192];
    size_t size;
} af_capture_t;

static int
af_write(char *data, int size, void *context)
{
    af_capture_t *out;

    out = context;
    if (size < 0 || (size_t)size >= sizeof(out->data) - out->size) {
        return -1;
    }
    memcpy(out->data + out->size, data, (size_t)size);
    out->size += (size_t)size;
    out->data[out->size] = '\0';
    return size;
}

int
test_animation_feedback(int cache, int soft)
{
    static struct { int flag; char const *value; } const options[] = {
        { '=', "1" }, { '~', "none" }, { 'd', "fs" }, { 'W', "gamma" },
        { 'X', "gamma" }, { '_', "none" }, { '.', "auto" },
        { 'Q', "kmeans:seed=1" }, { 'F', "none" }, { 'a', "off" },
        { '4', "full-frame" }, { '5', "none" }, { 'p', "2" },
        { 't', "rgb" }, { 'U', "gamma" }
    };
    static float const input[6] = { .55f, .4f, 0, 0, 1, 1 };
    static int const indexes[6] = { 1, 0, 0, 0, 1, 1 };
    sixel_encoder_t *encoder;
    sixel_output_t *output;
    sixel_frame_t *frame;
    sixel_dither_t *dither;
    sixel_palette_float32_entries_view_t view;
    af_capture_t capture;
    af_capture_t first;
    float source[18];
    float saved[18];
    unsigned char bytes[18];
    unsigned char *decoded;
    unsigned char *palette;
    double want;
    int width;
    int height;
    int colors;
    int low;
    int expected;
    int pass;
    int i;
    int c;
    int byte_frame;
    int ok;

    encoder = NULL;
    output = NULL;
    frame = NULL;
    decoded = NULL;
    palette = NULL;
    ok = 0;
    pass = 0;
    memset(&first, 0, sizeof(first));
    if (SIXEL_FAILED(sixel_encoder_new(&encoder, NULL)) ||
            SIXEL_FAILED(sixel_output_new(&output, af_write,
                &capture, NULL))) {
        goto end;
    }
    for (i = 0; i < (int)(sizeof(options) / sizeof(options[0])); ++i) {
        if (SIXEL_FAILED(sixel_encoder_setopt(encoder,
                options[i].flag, options[i].value))) {
            goto end;
        }
    }
    if (SIXEL_FAILED(sixel_encoder_setopt(encoder, '0',
            soft ? "soft" : "none"))) {
        goto end;
    }
    encoder->fstatic = 1;
    encoder->fignore_delay = 1;
    encoder->method_for_scan = SIXEL_SCAN_RASTER;
    for (pass = 0; pass < 3; ++pass) {
        memset(&capture, 0, sizeof(capture));
        byte_frame = cache && pass != 1;
        if (cache) {
            /* Exercise cache ownership at the internal selector boundary.
             * Public option conflict checks do not offer this as CLI syntax. */
            encoder->color_option = byte_frame ? SIXEL_COLOR_OPTION_BUILTIN
                : SIXEL_COLOR_OPTION_DEFAULT;
            encoder->builtin_palette = SIXEL_BUILTIN_G1;
        }
        for (i = 0; i < 6; ++i) {
            for (c = 0; c < 3; ++c) {
                source[3 * i + c] = pass == 1 && !cache
                    ? 1 - input[i] : input[i];
                bytes[3 * i + c] = indexes[i] ? 255 : 0;
            }
        }
        memcpy(saved, source, sizeof(saved));
        if (SIXEL_FAILED(sixel_frame_new(&frame, NULL)) ||
                SIXEL_FAILED(sixel_frame_init_borrowed(frame,
                    byte_frame ? (void *)bytes : (void *)source, 6, 1,
                    byte_frame ? SIXEL_PIXELFORMAT_RGB888
                        : SIXEL_PIXELFORMAT_RGBFLOAT32, NULL, -1))) {
            goto end;
        }
        sixel_frame_set_multiframe(frame, 1);
        sixel_frame_set_frame_no(frame, pass % 2);
        sixel_frame_set_loop_count(frame, pass / 2);
        if (SIXEL_FAILED(sixel_encoder_encode_frame(encoder, frame, output))) {
            goto end;
        }
        dither = (sixel_dither_t *)encoder->diagnostic_dither;
        if (dither == NULL || dither->use_rgb101_palette != !byte_frame ||
                memcmp(saved, source, sizeof(saved)) != 0 ||
                (byte_frame ? encoder->dither_cache != dither
                            : encoder->dither_cache != NULL)) {
            goto end;
        }
        if (!byte_frame) {
            if (SIXEL_FAILED(dither->palette->vtbl->get_entries_float32(
                    dither->palette, &view)) || view.entries == NULL ||
                    view.entry_count != 2) {
                goto end;
            }
            low = view.entries[0] < view.entries[3] ? 0 : 1;
            for (i = 0; i < 2; ++i) {
                /* Seed=1 converges to populations {0,0} and {.55,.4,1,1}.
                 * Their means are 0 and .7375; inversion gives .2625 and 1.
                 * A 1e-7 budget covers binary32 input and mean storage. */
                want = pass == 1 && !cache ? (i ? 1 : .2625)
                    : (i ? .7375 : 0);
                for (c = 0; c < 3; ++c) {
                    if (!isfinite(view.entries[3 * (i ? 1 - low : low) + c]) ||
                            fabs(view.entries[3 * (i ? 1 - low : low) + c]
                                - want) > 1e-7) {
                        goto end;
                    }
                }
            }
        }
        if (SIXEL_FAILED(sixel_decode_raw((unsigned char *)capture.data,
                (int)capture.size, &decoded, &width, &height,
                &palette, &colors, NULL)) || width != 6 || height != 1) {
            goto end;
        }
        for (i = 0; i < 6; ++i) {
            /* FS at pixel 2: .4 + (.55-.7375)*7/16 = .31796875,
             * below midpoint .36875. Without diffusion it selects high.
             * Compare decoded wire grays, independently of palette order. */
            expected = indexes[i] ? (byte_frame ? 255 : 189) : 0;
            if (pass == 1 && !cache) {
                expected = 255 - expected;
            }
            if (decoded[i] >= colors) {
                goto end;
            }
            for (c = 0; c < 3; ++c) {
                if (palette[3 * decoded[i] + c] != expected) {
                    fprintf(stderr, "pixel %d: %d expected %d\n", i,
                        palette[3 * decoded[i] + c], expected);
                    goto end;
                }
            }
        }
        if (pass == 0) {
            first = capture;
        } else if (pass == 2 && (capture.size != first.size ||
                memcmp(capture.data, first.data, capture.size) != 0)) {
            goto end;
        }
        free(decoded);
        free(palette);
        decoded = NULL;
        palette = NULL;
        sixel_frame_unref(frame);
        frame = NULL;
    }
    ok = 1;
end:
    if (!ok) {
        fprintf(stderr, "animation feedback cache=%d soft=%d pass=%d: %s\n",
            cache, soft, pass, sixel_helper_get_additional_message());
    }
    free(decoded);
    free(palette);
    sixel_frame_unref(frame);
    sixel_output_unref(output);
    sixel_encoder_unref(encoder);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}

int
test_animation_copy(int packed)
{
    sixel_encoding_planner_t planner;
    sixel_encoder_t *encoder;
    sixel_frame_t *frame;
    unsigned char pixels[8] = { 255, 0, 0, 128, 0, 255, 0, 0 };
    sixel_frame_transparency_t transparency;
    sixel_frame_interface_t *iface;
    int ok;

    encoder = NULL;
    frame = NULL;
    ok = 0;
    memset(&transparency, 0, sizeof(transparency));
    if (SIXEL_FAILED(sixel_encoder_new(&encoder, NULL)) ||
            SIXEL_FAILED(sixel_encoder_setopt(encoder, 'a', "off")) ||
            SIXEL_FAILED(sixel_encoder_setopt(encoder, 'F', "none")) ||
            SIXEL_FAILED(sixel_encoder_setopt(encoder, '_', "none")) ||
            SIXEL_FAILED(sixel_encoder_setopt(encoder, '~', "none")) ||
            SIXEL_FAILED(sixel_frame_new(&frame, NULL)) ||
            SIXEL_FAILED(sixel_frame_init_borrowed(frame, pixels, 2, 1,
                packed ? SIXEL_PIXELFORMAT_RGBA8888
                       : SIXEL_PIXELFORMAT_RGB888, NULL, -1))) {
        goto end;
    }
    sixel_frame_set_multiframe(frame, 1);
    iface = sixel_frame_as_interface(frame);
    transparency.transparent = -1;
    transparency.alpha_zero_is_transparent = packed;
    if (SIXEL_FAILED(iface->vtbl->set_transparency(iface, &transparency))) {
        goto end;
    }
    /* There is no explicit float request, crop, resize or workspace change.
     * Packed normalization still mutates frame storage and must get a copy.
     * Plain byte RGB must retain the zero-copy handoff opportunity. */
    sixel_encoding_planner_init(&planner);
    sixel_encoding_planner_analyze(&planner, encoder, frame);
    ok = planner.colorspace_active == packed && !planner.clip_active &&
        !planner.scale_active;
end:
    sixel_frame_unref(frame);
    sixel_encoder_unref(encoder);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}

int
test_animation_loader(char const *input, char const *path,
                       char const *loader, char const *threads, int p3)
{
    static struct { int flag; char const *value; } const options[] = {
        { '~', "none" }, { 'd', "fs" }, { 'W', "linear" },
        { 'X', "linear" }, { '_', "none" }, { '.', "auto" },
        { 'Q', "kmeans:seed=1" }, { 'F', "none" }, { 'a', "off" },
        { '4', "full-frame" }, { '5', "none" }, { 'p', "2" },
        { 't', "rgb" }, { 'g', NULL }, { 'l', "auto" }
    };
    /* Fixture: opaque red, then blue alpha=128/255 OVER red, two plays.
     * Linear composition yields (127/255,0,128/255). Decimal60 reference
     * using xy-derived primary matrices and sRGB transfer gives RGB101:
     * sRGB (100,0,0)/(73,0,74); P3 (92,20,14)/(67,14,71).
     * Values below include the decoder's final percent-to-byte rounding. */
    static unsigned char const expected[2][2][3] = {
        { { 255, 0, 0 }, { 186, 0, 189 } },
        { { 235, 51, 36 }, { 171, 36, 181 } }
    };
    sixel_encoder_t *encoder;
    af_capture_t capture;
    FILE *file;
    char *start;
    char *end;
    unsigned char *decoded;
    unsigned char *palette;
    int width;
    int height;
    int colors;
    int frames;
    int i;
    int c;
    int ok;

    encoder = NULL;
    file = NULL;
    decoded = NULL;
    palette = NULL;
    ok = 0;
    frames = 0;
    if (SIXEL_FAILED(sixel_encoder_new(&encoder, NULL))) {
        goto cleanup;
    }
    for (i = 0; i < (int)(sizeof(options) / sizeof(options[0])); ++i) {
        if (SIXEL_FAILED(sixel_encoder_setopt(encoder,
                options[i].flag, options[i].value))) {
            goto cleanup;
        }
    }
    if (SIXEL_FAILED(sixel_encoder_setopt(encoder, '=', threads)) ||
            SIXEL_FAILED(sixel_encoder_setopt(encoder, 'L', loader)) ||
            SIXEL_FAILED(sixel_encoder_setopt(encoder, 'U',
                p3 ? "display-p3" : "gamma")) ||
            SIXEL_FAILED(sixel_encoder_setopt(encoder, 'o', path)) ||
            SIXEL_FAILED(sixel_encoder_encode(encoder, input))) {
        goto cleanup;
    }
    /* Close the writer before inspecting every DCS, not just the final
     * diagnostic palette. Cursor placement bytes may surround each DCS. */
    sixel_encoder_unref(encoder);
    encoder = NULL;
    file = sixel_compat_fopen(path, "rb");
    if (file == NULL) {
        goto cleanup;
    }
    capture.size = fread(capture.data, 1, sizeof(capture.data) - 1, file);
    if (ferror(file) || !feof(file)) {
        goto cleanup;
    }
    capture.data[capture.size] = '\0';
    start = capture.data;
    while ((start = strstr(start, "\033P")) != NULL) {
        end = strstr(start + 2, "\033\\");
        if (end == NULL || frames >= 4 ||
                SIXEL_FAILED(sixel_decode_raw((unsigned char *)start,
                    (int)(end + 2 - start), &decoded, &width, &height,
                    &palette, &colors, NULL)) || width != 8 || height != 8) {
            goto cleanup;
        }
        for (i = 0; i < 64; ++i) {
            if (decoded[i] >= colors) {
                goto cleanup;
            }
            for (c = 0; c < 3; ++c) {
                if (palette[decoded[i] * 3 + c] !=
                        expected[p3][frames % 2][c]) {
                    fprintf(stderr, "frame=%d pixel=%d channel=%d got=%d\n",
                        frames, i, c, palette[decoded[i] * 3 + c]);
                    goto cleanup;
                }
            }
        }
        ++frames;
        start = end + 2;
        free(decoded);
        free(palette);
        decoded = NULL;
        palette = NULL;
    }
    ok = frames == 4;
cleanup:
    if (!ok) {
        fprintf(stderr, "animation loader frames=%d: %s\n", frames,
            sixel_helper_get_additional_message());
    }
    if (file != NULL) {
        fclose(file);
    }
    free(decoded);
    free(palette);
    sixel_encoder_unref(encoder);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
