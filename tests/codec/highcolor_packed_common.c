/* SPDX-License-Identifier: MIT */
/* Independent packed-frame normalization and paint-time output witnesses. */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sixel.h>
#include "src/encoder.h"
#include "src/sixel_decode_pixels.h"

typedef struct hcp_stream {
    unsigned char bytes[8192];
    size_t size;
} hcp_stream_t;

static int hcp_fail_after;

static void *
hcp_malloc(size_t size)
{
    if (hcp_fail_after > 0 && --hcp_fail_after == 0) {
        return NULL;
    }
    return malloc(size);
}

static int
hcp_write(char *data, int size, void *context)
{
    hcp_stream_t *stream;

    stream = context;
    if (size < 0 || (size_t)size >= sizeof(stream->bytes) - stream->size) {
        return -1;
    }
    memcpy(stream->bytes + stream->size, data, (size_t)size);
    stream->size += (size_t)size;
    return size;
}

int
test_highcolor_packed(int scenario)
{
    static int const formats[] = {
        SIXEL_PIXELFORMAT_RGBA8888, SIXEL_PIXELFORMAT_ARGB8888,
        SIXEL_PIXELFORMAT_BGRA8888, SIXEL_PIXELFORMAT_ABGR8888,
        SIXEL_PIXELFORMAT_GA88, SIXEL_PIXELFORMAT_AG88
    };
    static char const *const spaces[] = {
        "gamma", "gamma", "linear", "oklab", "cielab", "din99d"
    };
    static unsigned char const rgb[4][3] = {
        {17, 91, 203}, {201, 73, 19}, {5, 151, 227}, {43, 99, 177}
    };
    static unsigned char const background[3] = {61, 129, 233};
    /* Independent D65 sRGB -> Display P3 primary matrix. The oracle uses
     * double analytic transfer functions, never production conversions.
     * A checked .002 percentage-unit distance from wire rounding ties
     * exceeds the float32 workspace round-trip budget (< .001 units).
     * Rational blends checked before a workspace transform have only a
     * final float store: 6e-8 absolute error covers unit-range rounding. */
    static double const matrix[3][3] = {
        {.822461968714362, .177538031285638, 0},
        {.033194198850962, .966805801149038, 0},
        {.017082630721120, .072397440663963, .910519928614917}
    };
    sixel_encoder_t *encoder;
    sixel_output_t *output;
    sixel_frame_t *frame;
    sixel_frame_interface_t *iface;
    sixel_frame_transparency_t transparency;
    sixel_allocator_t *allocator;
    sixel_encoding_planner_t planner;
    SIXELSTATUS status;
    hcp_stream_t stream;
    unsigned char input[16];
    unsigned char saved[16];
    unsigned char alpha[4];
    unsigned char *mask;
    unsigned char *decoded;
    unsigned char *palette;
    float const *values;
    double expected[4][3];
    double linear[3];
    double value;
    double bg;
    unsigned int flags;
    int fields[4][3];
    int omitted[4];
    int layout;
    int use_bg;
    int interpret;
    int p3;
    int binary;
    int want_float;
    int width;
    int w;
    int h;
    int ncolors;
    int phase;
    int i;
    int c;
    int j;
    int offset;
    int source;
    int want;
    int ok;

    encoder = NULL;
    output = NULL;
    frame = NULL;
    allocator = NULL;
    mask = decoded = palette = NULL;
    ok = 0;
    hcp_fail_after = 0;
    layout = scenario <= 71 ? scenario - 66 : 0;
    use_bg = layout >= 4 || scenario == 76;
    binary = scenario == 72 || scenario == 73 || scenario == 82;
    width = scenario == 77 ? 3 : scenario == 78 ? 8 : 4;
    if (SIXEL_FAILED(sixel_allocator_new(&allocator, hcp_malloc,
                NULL, NULL, NULL)) ||
            SIXEL_FAILED(sixel_encoder_new(&encoder, NULL)) ||
            SIXEL_FAILED(sixel_output_new(&output, hcp_write, &stream,
                NULL)) ||
            SIXEL_FAILED(sixel_encoder_setopt(encoder, 'I', NULL)) ||
            SIXEL_FAILED(sixel_encoder_setopt(encoder, '.',
                scenario == 73 ? "float32" : "8bit")) ||
            SIXEL_FAILED(sixel_encoder_setopt(encoder, 'W', spaces[layout])) ||
            SIXEL_FAILED(sixel_encoder_setopt(encoder, 't', "rgb")) ||
            SIXEL_FAILED(sixel_encoder_setopt(encoder, '=', "1")) ||
            SIXEL_FAILED(sixel_encoder_setopt(encoder, '_', "none"))) {
        goto end;
    }
    /* High color bypasses clustering. An irrelevant non-gamma -X must not
     * promote an otherwise exact binary-alpha job to float. */
    if (scenario == 72 &&
            SIXEL_FAILED(sixel_encoder_setopt(encoder, 'X', "cielab"))) {
        goto end;
    }
    if (use_bg && SIXEL_FAILED(sixel_encoder_setopt(encoder, 'B',
            "rgb:3d/81/e9"))) {
        goto end;
    }
    if (scenario == 77 && SIXEL_FAILED(sixel_encoder_setopt(encoder, 'c',
            "3x1+1+0"))) {
        goto end;
    }
    if (scenario == 78 &&
            (SIXEL_FAILED(sixel_encoder_setopt(encoder, 'w', "8")) ||
             SIXEL_FAILED(sixel_encoder_setopt(encoder, 'h', "1")) ||
             SIXEL_FAILED(sixel_encoder_setopt(encoder, 'r', "nearest")))) {
        goto end;
    }
    for (phase = 0; phase < (scenario == 79 ? 3 : 1); ++phase) {
        p3 = (scenario >= 67 && scenario <= 71) ||
            (scenario == 79 && phase != 1);
        interpret = scenario != 74 && !(scenario == 79 && phase == 1);
        want_float = p3 || scenario == 73 || (interpret && !binary);
        if (SIXEL_FAILED(sixel_encoder_setopt(encoder, 'U',
                p3 ? "display-p3" : "gamma"))) {
            goto end;
        }
        alpha[0] = 0;
        alpha[1] = binary ? 255 : 1;
        alpha[2] = binary ? 255 : 128;
        alpha[3] = 255;
        memset(input, 0, sizeof(input));
        for (i = 0; i < 4; ++i) {
            offset = i * (layout < 4 ? 4 : 2);
            switch (layout) {
            case 0:
                memcpy(input + offset, rgb[i], 3);
                input[offset + 3] = alpha[i];
                break;
            case 1:
                input[offset] = alpha[i];
                memcpy(input + offset + 1, rgb[i], 3);
                break;
            case 2:
            case 3:
                for (c = 0; c < 3; ++c) {
                    input[offset + c + (layout == 3)] = rgb[i][2 - c];
                }
                input[offset + (layout == 2 ? 3 : 0)] = alpha[i];
                break;
            case 4:
            case 5:
                input[offset + (layout == 5)] = rgb[i][0];
                input[offset + (layout == 4)] = alpha[i];
                break;
            }
            omitted[i] = (interpret && alpha[i] == 0) ||
                ((scenario == 74 || scenario == 75) && i == 3);
            for (c = 0; c < 3; ++c) {
                value = rgb[i][layout < 4 ? c : 0] / 255.0;
                /* The chosen background has legacy integer gray 121. */
                bg = use_bg ? (layout < 4 ? background[c] : 121) : 0;
                if (interpret && !omitted[i] && alpha[i] < 255) {
                    value = (rgb[i][layout < 4 ? c : 0] * alpha[i] +
                        bg * (255 - alpha[i])) / 65025.0;
                }
                expected[i][c] = value;
                linear[c] = value <= .04045 ? value / 12.92 :
                    pow((value + .055) / 1.055, 2.4);
            }
            for (c = 0; c < 3; ++c) {
                value = expected[i][c];
                if (p3) {
                    value = 0;
                    for (j = 0; j < 3; ++j) {
                        value += matrix[c][j] * linear[j];
                    }
                    value = value <= .0031308 ? 12.92 * value :
                        1.055 * pow(value, 1.0 / 2.4) - .055;
                }
                value *= 100;
                if (fabs(value - floor(value) - .5) < .002) {
                    goto end;
                }
                fields[i][c] = (int)(value + .5);
            }
        }
        memcpy(saved, input, sizeof(input));
        if (SIXEL_FAILED(sixel_frame_new(&frame, allocator)) ||
                SIXEL_FAILED(sixel_frame_init_borrowed(frame, input, 4, 1,
                    formats[layout], NULL, -1))) {
            goto end;
        }
        iface = sixel_frame_as_interface(frame);
        memset(&transparency, 0, sizeof(transparency));
        transparency.transparent = -1;
        transparency.alpha_zero_is_transparent = interpret;
        if (scenario == 74 || scenario == 75) {
            mask = calloc(4, 1);
            if (mask == NULL) {
                goto end;
            }
            mask[3] = 1;
            transparency.transparent_mask = mask;
            transparency.transparent_mask_size = 4;
        }
        if (SIXEL_FAILED(iface->vtbl->set_transparency(iface,
                &transparency))) {
            goto end;
        }
        mask = NULL;
        if (scenario == 82) {
            sixel_frame_set_multiframe(frame, 1);
            sixel_encoding_planner_init(&planner);
            sixel_encoding_planner_analyze(&planner, encoder, frame);
            /* Packed separation mutates storage even without explicit
             * float, crop or resize. Shared handoff must clone first. */
            ok = planner.colorspace_active && !planner.clip_active &&
                !planner.scale_active;
            goto end;
        }
        stream.size = 0;
        if (scenario == 80 || scenario == 81) {
            /* Only frame allocation is faulted, not encoder setup. */
            hcp_fail_after = scenario - 79;
            status = sixel_encoder_encode_frame(encoder, frame, output);
            if (status != SIXEL_BAD_ALLOCATION || stream.size != 0 ||
                    sixel_frame_get_pixels(frame) != input ||
                    sixel_frame_get_pixelformat(frame) != formats[layout] ||
                    SIXEL_FAILED(iface->vtbl->get_transparency(iface,
                        &transparency)) ||
                    transparency.transparent_mask != NULL ||
                    !transparency.alpha_zero_is_transparent) {
                goto end;
            }
            hcp_fail_after = 0;
        }
        if (SIXEL_FAILED(sixel_encoder_encode_frame(encoder, frame, output)) ||
                memcmp(input, saved, sizeof(input)) != 0) {
            goto end;
        }
        if (layout <= 1 && scenario != 77 && scenario != 78) {
            if (sixel_frame_get_pixelformat(frame) != (want_float ?
                    SIXEL_PIXELFORMAT_RGBFLOAT32 : SIXEL_PIXELFORMAT_RGB888)) {
                fprintf(stderr, "packed %d: unexpected normalized format\n",
                    scenario);
                goto end;
            }
            if (want_float) {
                values = (float const *)sixel_frame_get_pixels(frame);
                for (i = 0; i < 4; ++i) {
                    for (c = 0; c < 3; ++c) {
                        if (!isfinite(values[3 * i + c]) ||
                                fabs(values[3 * i + c] - expected[i][c]) >
                                    6e-8) {
                            fprintf(stderr, "packed %d blend %d/%d: "
                                "%.9g want %.9g\n", scenario, i, c,
                                (double)values[3 * i + c], expected[i][c]);
                            goto end;
                        }
                    }
                }
            }
        }
        if (SIXEL_FAILED(sixel_decode_direct_with_options(stream.bytes,
                    (int)stream.size, 0, &decoded, &w, &h, &palette,
                    &ncolors, &flags, NULL)) || w != width || h != 1) {
            goto end;
        }
        for (i = 0; i < width; ++i) {
            source = scenario == 77 ? i + 1 : scenario == 78 ? i / 2 : i;
            if (decoded[4 * i + 3] != (omitted[source] ? 0 : 255)) {
                goto end;
            }
            if (omitted[source]) {
                continue;
            }
            for (c = 0; c < 3; ++c) {
                want = (fields[source][c] * 255 + 50) / 100;
                if (decoded[4 * i + c] != want) {
                    fprintf(stderr, "packed %d phase %d pixel %d/%d: "
                        "%u want %d\n", scenario, phase, i, c,
                        decoded[4 * i + c], want);
                    goto end;
                }
            }
        }
        free(decoded);
        free(palette);
        decoded = palette = NULL;
        sixel_frame_unref(frame);
        frame = NULL;
    }
    ok = 1;
end:
    hcp_fail_after = 0;
    if (!ok) {
        fprintf(stderr, "high-color packed %d: %s\n", scenario,
            sixel_helper_get_additional_message());
    }
    free(mask);
    free(decoded);
    free(palette);
    sixel_frame_unref(frame);
    sixel_output_unref(output);
    sixel_encoder_unref(encoder);
    sixel_allocator_unref(allocator);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
