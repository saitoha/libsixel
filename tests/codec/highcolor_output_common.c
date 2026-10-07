/* SPDX-License-Identifier: MIT */
/* Test final samples, not the final register snapshot. The latter loses
 * earlier paint colors when high-color output redefines a register. */
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
#include "src/output.h"
#include "src/sixel_decode_pixels.h"

typedef struct hc_stream {
    unsigned char bytes[131072];
    size_t size;
} hc_stream_t;

static int
hc_write(char *data, int size, void *context)
{
    hc_stream_t *stream;

    stream = context;
    if (size < 0 || (size_t)size >= sizeof(stream->bytes) - stream->size) {
        return -1;
    }
    memcpy(stream->bytes + stream->size, data, (size_t)size);
    stream->size += (size_t)size;
    stream->bytes[stream->size] = 0;
    return size;
}

static int
hc_decode(hc_stream_t *stream, int width, int height, int const *expected,
          int multi, int redefine)
{
    sixel_allocator_t *allocator;
    unsigned char *pixels;
    unsigned char *palette;
    unsigned int flags;
    int w;
    int h;
    int ncolors;
    int i;
    int c;
    int want;
    int ok;

    allocator = NULL;
    pixels = palette = NULL;
    w = h = 0;
    ok = 0;
    if (SIXEL_FAILED(sixel_allocator_new(&allocator, NULL, NULL,
                NULL, NULL)) ||
            SIXEL_FAILED(sixel_decode_direct_with_options(stream->bytes,
                (int)stream->size, 0, &pixels, &w, &h, &palette, &ncolors,
                &flags, allocator)) || w != width || h != height) {
        fprintf(stderr, "decode dimensions %dx%d expected %dx%d\n",
            w, h, width, height);
        goto end;
    }
    for (i = 0; i < width * height; ++i) {
        for (c = 0; c < 3; ++c) {
            want = expected[multi ? i * 3 + c : c];
            want = (want * 255 + 50) / 100;
            if (pixels[i * 4 + c] != want || pixels[i * 4 + 3] != 255) {
                fprintf(stderr, "pixel %d channel %d: %u expected %d\n",
                    i, c, pixels[i * 4 + c], want);
                goto end;
            }
        }
    }
    if (redefine && (flags & SIXEL_DECODE_PIXELS_RESULT_PALETTE_REDEFINED)
            == 0U) {
        fprintf(stderr, "multi-pass fixture did not redefine registers\n");
        goto end;
    }
    ok = 1;
end:
    sixel_allocator_free(allocator, pixels);
    sixel_allocator_free(allocator, palette);
    sixel_allocator_unref(allocator);
    return ok;
}

int
test_highcolor_output(int scenario)
{
    /* Independent analytic references: sRGB transfer at .25/.5/.75,
     * inverse transfer at 64/128/192 over 255, and the documented SMPTE-C
     * matrix at (.5, -.9, .5). All finite fixtures are away from percentage
     * ties: transformed samples have > .02 field units of margin, and the
     * multipass samples have > .0098, versus a conservative 1e-4 propagated
     * arithmetic budget. The subbyte witness has .1 field
     * margin and intentionally crosses a bin after an RGB888 store. */
    static int const byte_rgb[3] = { 25, 50, 75 };
    static int const linear_rgb[3] = { 5, 22, 53 };
    static int const subbyte_rgb[3] = { 12, 23, 34 };
    static int const gamma_rgb[3] = { 54, 74, 88 };
    static int const smptec_rgb[3] = { 78, 0, 73 };
    static int const hls_rgb[3] = { 23, 23, 23 };
    static int const diffusion_rgb[3] = { 13, 24, 35 };
    unsigned char bytes[4];
    float samples[32 * 24 * 3];
    float saved[32 * 24 * 3];
    int multi_expected[32 * 24 * 3];
    int const *expected;
    unsigned char *input;
    sixel_dither_t *dither;
    sixel_encoder_t *encoder;
    sixel_frame_t *frame;
    sixel_output_t *output;
    hc_stream_t stream;
    SIXELSTATUS status;
    int format;
    int source;
    int target;
    int width;
    int height;
    int i;
    int c;
    int step;
    int steps;
    int key_index;
    int ok;

    dither = NULL;
    encoder = NULL;
    frame = NULL;
    output = NULL;
    ok = 0;
    bytes[0] = 64; bytes[1] = 128; bytes[2] = 192; bytes[3] = 255;
    samples[0] = .124f; samples[1] = .234f; samples[2] = .344f;
    width = height = 1;
    format = SIXEL_PIXELFORMAT_RGBFLOAT32;
    source = target = SIXEL_COLORSPACE_GAMMA;
    input = (unsigned char *)samples;
    expected = subbyte_rgb;
    if (scenario == 1 || scenario == 2 || scenario == 9) {
        format = scenario == 9 ? SIXEL_PIXELFORMAT_RGBA8888 :
            SIXEL_PIXELFORMAT_RGB888;
        input = bytes;
        target = scenario == 1 ? SIXEL_COLORSPACE_GAMMA :
            SIXEL_COLORSPACE_LINEAR;
        expected = scenario == 1 ? byte_rgb : linear_rgb;
    } else if (scenario == 4) {
        source = SIXEL_COLORSPACE_LINEAR;
        format = SIXEL_PIXELFORMAT_LINEARRGBFLOAT32;
        samples[0] = .25f; samples[1] = .5f; samples[2] = .75f;
        expected = gamma_rgb;
    } else if (scenario == 5 || scenario == 11) {
        source = SIXEL_COLORSPACE_LINEAR;
        target = SIXEL_COLORSPACE_SMPTEC;
        format = SIXEL_PIXELFORMAT_LINEARRGBFLOAT32;
        samples[0] = .5f; samples[1] = -.9f; samples[2] = .5f;
        expected = smptec_rgb;
    } else if (scenario == 6) {
        samples[0] = samples[1] = samples[2] = .234f;
        expected = hls_rgb;
    } else if (scenario == 12) {
        width = 32;
        height = 18;
        for (i = 0; i < width * height; ++i) {
            /* The last band also needs palette reuse: every source pixel
             * must retain its representative until it is actually painted. */
            key_index = i;
            samples[i * 3] = (float)((key_index % 32) * 8 + 2) / 255.0f;
            samples[i * 3 + 1] = (float)((key_index / 32) * 8 + 2) / 255.0f;
            samples[i * 3 + 2] = .234f;
            for (c = 0; c < 3; ++c) {
                multi_expected[i * 3 + c] = c == 2 ? 23 :
                    (((c == 0 ? key_index % 32 : key_index / 32) * 8 + 2)
                        * 100 + 127)
                        / 255;
            }
        }
        expected = multi_expected;
    } else if (scenario == 13) {
        expected = diffusion_rgb;
    }
    memcpy(saved, samples, (size_t)width * (size_t)height * 3 * sizeof(float));
    memset(&stream, 0, sizeof(stream));
    if (SIXEL_FAILED(sixel_output_new(&output, hc_write, &stream, NULL))) {
        goto end;
    }
    sixel_output_set_palette_type(output, scenario == 6 ?
        SIXEL_PALETTETYPE_HLS : SIXEL_PALETTETYPE_RGB);
    if (scenario == 10 || scenario == 11) {
        if (SIXEL_FAILED(sixel_encoder_new(&encoder, NULL)) ||
                SIXEL_FAILED(sixel_encoder_setopt(encoder, 'I', NULL)) ||
                SIXEL_FAILED(sixel_encoder_setopt(encoder, '=', "1")) ||
                SIXEL_FAILED(sixel_encoder_setopt(encoder, '.', "8bit")) ||
                SIXEL_FAILED(sixel_encoder_setopt(encoder, 'W',
                    scenario == 11 ? "linear" : "gamma")) ||
                SIXEL_FAILED(sixel_encoder_setopt(encoder, 'U',
                    scenario == 11 ? "smpte-c" : "gamma")) ||
                SIXEL_FAILED(sixel_frame_new(&frame, NULL)) ||
                SIXEL_FAILED(sixel_frame_init_borrowed(frame, input, width,
                    height, format, NULL, -1)) ||
                SIXEL_FAILED(sixel_encoder_encode_frame(encoder, frame,
                    output)) || encoder->diagnostic_dither == NULL ||
                !SIXEL_PIXELFORMAT_IS_FLOAT32(
                    sixel_frame_get_pixelformat(frame))) {
            goto end;
        }
        ok = hc_decode(&stream, width, height, expected, 0, 0);
        goto end;
    }
    if (SIXEL_FAILED(sixel_dither_new(&dither, -1, NULL))) {
        goto end;
    }
    sixel_dither_set_diffusion_type(dither, scenario == 13 ?
        SIXEL_DIFFUSE_FS : SIXEL_DIFFUSE_NONE);
    steps = scenario == 14 ? 3 : 1;
    for (step = 0; step < steps; ++step) {
        if (scenario == 14 && step != 0) {
            input = bytes;
            format = SIXEL_PIXELFORMAT_RGB888;
            target = step == 1 ? SIXEL_COLORSPACE_LINEAR :
                SIXEL_COLORSPACE_GAMMA;
            expected = step == 1 ? linear_rgb : byte_rgb;
        }
        stream.size = 0;
        sixel_dither_set_pixelformat(dither, format);
        if (SIXEL_FAILED(sixel_output_set_encoder_format(output,
                format, source, target))) {
            goto end;
        }
        if (scenario == 7 || scenario == 8) {
            samples[1] = scenario == 7 ? NAN : INFINITY;
            status = sixel_encode(input, width, height, 0, dither, output);
            if (status != SIXEL_BAD_INPUT || stream.size != 0 ||
                    dither->pixelformat != format) {
                goto end;
            }
            samples[1] = saved[1];
        }
        status = sixel_encode(input, width, height, 0, dither, output);
        if (SIXEL_FAILED(status) || dither->pixelformat != format ||
                memcmp(samples, saved, (size_t)width * (size_t)height *
                    3 * sizeof(float)) != 0 ||
                !hc_decode(&stream, width, height, expected,
                    scenario == 12, scenario == 12)) {
            goto end;
        }
    }
    ok = 1;
end:
    if (!ok) {
        fprintf(stderr, "high-color scenario %d: %s\n", scenario,
            sixel_helper_get_additional_message());
    }
    sixel_frame_unref(frame);
    sixel_encoder_unref(encoder);
    sixel_dither_unref(dither);
    sixel_output_unref(output);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}

/* Independent P3 primary references use the D65 chromaticities and sRGB
 * transfer, as in the color-numeric fixture generator. The third sample is
 * P3 (.25,.5,.75). Matrix constants below map linear P3 to linear sRGB;
 * neither fixture construction nor expected fields calls production CMS.
 * At the RGB101 boundary all ordinary samples have >= .5 field margin;
 * neutral .234 has .1 and the multi-pass grid has > .0098. A conservative
 * 1e-3 field-unit budget covers float storage, matrix/transfer arithmetic
 * and the approximate perceptual-space inverses. These margins also reject
 * premature RGB888 storage for the neutral witness. */
static double const hc_p3_work[5][3][3] = {
    { { 1.0930663624352, -.22674197356975, -.15013458093712 },
      { -.51160498258534, 1.0182656579378, -.31067462129058 },
      { .12407596556951, .50734576865241, .77112740734811 } },
    { { 1.2249401762806, -.042056954709688, -.019637554590334 },
      { -.22494017628056, 1.0420569547097, -.078636045550632 },
      { .014173712541967, .22090336569747, .55604119739664 } },
    { { .64857407194143, .26204175072605, .14500192827479 },
      { .8488292869841, -.30424052175479, .20796743046686 },
      { .58353085687825, -.057052070168363, -.12120522630902 } },
    { { .54970952834149, .7351091454253, .74035572897474 },
      { .86588907613231, -.90066633129596, .84112404854987 },
      { .51964299881485, -.024190924241413, -.33268467557514 } },
    { { .58723742460956, .40023305508709, .32314385132397 },
      { .88244968609601, -.3801545974064, .38760984200729 },
      { .55771760713899, -.16217401415405, -.26830746085521 } }
};

int
test_highcolor_p3(int scenario)
{
    static char const *const spaces[5] = {
        "gamma", "linear", "oklab", "cielab", "din99d"
    };
    static int const formats[5] = {
        SIXEL_PIXELFORMAT_RGBFLOAT32, SIXEL_PIXELFORMAT_LINEARRGBFLOAT32,
        SIXEL_PIXELFORMAT_OKLABFLOAT32, SIXEL_PIXELFORMAT_CIELABFLOAT32,
        SIXEL_PIXELFORMAT_DIN99DFLOAT32
    };
    static double const matrix[3][3] = {
        { 1.2249401762806, -.22494017628056, 0 },
        { -.042056954709688, 1.0420569547097, 0 },
        { -.019637554590334, -.078636045550632, 1.098273600140966 }
    };
    static int const fields[9] = {100, 0, 0, 0, 100, 0, 25, 50, 75};
    sixel_encoder_t *encoder;
    sixel_encoder_t *other;
    sixel_output_t *output;
    sixel_frame_t *frame;
    sixel_frame_interface_t *iface;
    sixel_frame_transparency_t transparency;
    hc_stream_t stream;
    float samples[32 * 24 * 3];
    float saved[32 * 24 * 3];
    int expected[32 * 24 * 3];
    unsigned char bytes[4];
    unsigned char *mask;
    unsigned char *input;
    double device[3];
    double value;
    SIXELSTATUS status;
    int space;
    int format;
    int width;
    int height;
    int phase;
    int phases;
    int reject;
    int i;
    int c;
    int j;
    int k;
    int ok;

    encoder = other = NULL;
    output = NULL;
    frame = NULL;
    mask = NULL;
    ok = 0;
    memset(&stream, 0, sizeof(stream));
    if (SIXEL_FAILED(sixel_encoder_new(&encoder, NULL)) ||
            SIXEL_FAILED(sixel_output_new(&output, hc_write, &stream, NULL)) ||
            SIXEL_FAILED(sixel_encoder_setopt(encoder, 'I', NULL)) ||
            SIXEL_FAILED(sixel_encoder_setopt(encoder, 'U', "display-p3")) ||
            SIXEL_FAILED(sixel_encoder_setopt(encoder, '=',
                scenario == 24 ? "4" : "1"))) {
        goto end;
    }
    space = scenario >= 15 && scenario <= 19 ? scenario - 15 : 0;
    if (scenario == 23 || scenario == 24) {
        space = 1;
    }
    if (SIXEL_FAILED(sixel_encoder_setopt(encoder, 'W', spaces[space]))) {
        goto end;
    }
    if (scenario == 22 &&
            SIXEL_FAILED(sixel_encoder_setopt(encoder, 't', "hls"))) {
        goto end;
    }
    phases = scenario >= 26 ? 2 : scenario == 25 ? 3 : 1;
    if (scenario == 25 &&
            (SIXEL_FAILED(sixel_encoder_new(&other, NULL)) ||
             SIXEL_FAILED(sixel_encoder_setopt(other, 'I', NULL)) ||
             SIXEL_FAILED(sixel_encoder_setopt(other, '=', "1")) ||
             SIXEL_FAILED(sixel_encoder_setopt(other, 'U', "gamma")))) {
        goto end;
    }
    for (phase = 0; phase < phases; ++phase) {
        width = scenario <= 19 ? 3 : 1;
        height = 1;
        format = formats[space];
        input = (unsigned char *)samples;
        for (i = 0; i < width * 3; ++i) {
            samples[i] = width == 3 ?
                (float)hc_p3_work[space][i / 3][i % 3] : .234f;
            expected[i] = width == 3 ? fields[i] : 23;
        }
        if (scenario == 20 || (scenario == 25 && phase == 1)) {
            bytes[0] = 255; bytes[1] = bytes[2] = 0;
            format = SIXEL_PIXELFORMAT_RGB888;
            input = bytes;
            /* sRGB red in Display P3, independent chromaticity matrix.
             * Fields have > .23 margin against a 1e-3 field budget. */
            expected[0] = 92; expected[1] = 20; expected[2] = 14;
        }
        if (scenario == 23 || scenario == 24) {
            width = 32;
            height = 18;
            for (i = 0; i < width * height; ++i) {
                /* Include the final pending pass in the P3 lifetime check. */
                k = i;
                device[0] = ((k % 32) * 8 + 2) / 255.0;
                device[1] = ((k / 32) * 8 + 2) / 255.0;
                device[2] = .234;
                for (c = 0; c < 3; ++c) {
                    expected[i * 3 + c] = (int)(device[c] * 100.0 + .5);
                    device[c] = device[c] <= .04045 ? device[c] / 12.92 :
                        pow((device[c] + .055) / 1.055, 2.4);
                }
                for (c = 0; c < 3; ++c) {
                    value = 0;
                    for (j = 0; j < 3; ++j) {
                        value += matrix[c][j] * device[j];
                    }
                    samples[i * 3 + c] = (float)value;
                }
            }
        }
        if (scenario == 25) {
            samples[0] = 1.0f; samples[1] = samples[2] = 0.0f;
            expected[0] = 92; expected[1] = 20; expected[2] = 14;
            if (SIXEL_FAILED(sixel_encoder_setopt(encoder, 'U',
                    phase == 1 ? "gamma" : "display-p3"))) {
                goto end;
            }
            if (phase == 1) {
                expected[0] = 100; expected[1] = expected[2] = 0;
            }
        }
        reject = scenario >= 26 && phase == 0;
        if (reject && scenario == 33) {
            samples[0] = NAN;
        }
        if (reject && scenario == 34) {
            bytes[0] = bytes[1] = bytes[2] = 60; bytes[3] = 0;
            input = bytes;
            format = SIXEL_PIXELFORMAT_RGBA8888;
        }
        memcpy(saved, samples, (size_t)width * (size_t)height * 3U *
            sizeof(float));
        if (SIXEL_FAILED(sixel_frame_new(&frame, NULL)) ||
                SIXEL_FAILED(sixel_frame_init_borrowed(frame, input, width,
                    height, format, NULL, -1))) {
            goto end;
        }
        if (reject && scenario == 26) {
            mask = calloc(1, 1);
            if (mask == NULL) {
                goto end;
            }
            mask[0] = 1;
            memset(&transparency, 0, sizeof(transparency));
            transparency.transparent = -1;
            transparency.alpha_zero_is_transparent = 1;
            transparency.transparent_mask = mask;
            transparency.transparent_mask_size = 0;
            iface = (sixel_frame_interface_t *)frame;
            if (SIXEL_FAILED(iface->vtbl->set_transparency(iface,
                    &transparency))) {
                goto end;
            }
            mask = NULL;
        }
        /* Exercise frame-entry guards independently of option ordering. */
        encoder->sixdelta_enabled = reject && scenario == 27;
        encoder->ormode = reject && scenario == 28;
        encoder->fdrcs = reject && scenario == 29;
        encoder->fuse_macro = reject && scenario == 30;
        encoder->output_is_png = reject && scenario == 31;
        if (scenario == 32 && SIXEL_FAILED(sixel_encoder_setopt(encoder,
                '_', reject ? "nearest" : "none"))) {
            goto end;
        }
        stream.size = 0;
        status = sixel_encoder_encode_frame(encoder, frame, output);
        if (reject) {
            if (status != (scenario == 26 || scenario >= 33 ?
                    SIXEL_BAD_INPUT : SIXEL_BAD_ARGUMENT) ||
                    stream.size != 0) {
                goto end;
            }
        } else if (SIXEL_FAILED(status) ||
                !hc_decode(&stream, width, height, expected, 1,
                    scenario == 23 || scenario == 24)) {
            goto end;
        }
        /* The neutral value alone cannot distinguish RGB from HLS syntax.
         * Require the HLS fields as well as their direct-decoded paint. */
        if (scenario == 22 && strstr((char *)stream.bytes,
                "#0;1;0;23;0") == NULL) {
            goto end;
        }
        if (memcmp(samples, saved, (size_t)width * (size_t)height * 3U *
                sizeof(float)) != 0) {
            goto end;
        }
        sixel_frame_unref(frame);
        frame = NULL;
        if (scenario == 25) {
            /* A second encoder shares the sink, never the P3 intent. */
            samples[0] = 1.0f; samples[1] = samples[2] = 0.0f;
            expected[0] = 100; expected[1] = expected[2] = 0;
            stream.size = 0;
            if (SIXEL_FAILED(sixel_frame_new(&frame, NULL)) ||
                    SIXEL_FAILED(sixel_frame_init_borrowed(frame, samples,
                        1, 1, SIXEL_PIXELFORMAT_RGBFLOAT32, NULL, -1)) ||
                    SIXEL_FAILED(sixel_encoder_encode_frame(other, frame,
                        output)) ||
                    !hc_decode(&stream, 1, 1, expected, 1, 0)) {
                goto end;
            }
            sixel_frame_unref(frame);
            frame = NULL;
        }
    }
    ok = 1;
end:
    if (!ok) {
        fprintf(stderr, "high-color P3 scenario %d: %s\n", scenario,
            sixel_helper_get_additional_message());
    }
    free(mask);
    sixel_frame_unref(frame);
    sixel_encoder_unref(encoder);
    sixel_encoder_unref(other);
    sixel_output_unref(output);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
