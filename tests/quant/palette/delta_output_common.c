/* SPDX-License-Identifier: MIT */
/* Delta output coordinates, independent references and plane ownership. */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sixel.h>
#include "src/colorspace.h"
#include "tests/data/color-numeric/output-native.h"
#include "src/dither.h"
#include "src/encoder.h"
#include "src/palette.h"
#include "src/sixel_decode_pixels.h"

static int const pd_workspaces[5] = {
    SIXEL_COLORSPACE_GAMMA, SIXEL_COLORSPACE_LINEAR, SIXEL_COLORSPACE_OKLAB,
    SIXEL_COLORSPACE_CIELAB, SIXEL_COLORSPACE_DIN99D
};
static int const pd_formats[5] = {
    SIXEL_PIXELFORMAT_RGBFLOAT32, SIXEL_PIXELFORMAT_LINEARRGBFLOAT32,
    SIXEL_PIXELFORMAT_OKLABFLOAT32, SIXEL_PIXELFORMAT_CIELABFLOAT32,
    SIXEL_PIXELFORMAT_DIN99DFLOAT32
};
static char const *const pd_spaces[5] = {
    "gamma", "linear", "oklab", "cielab", "din99d"
};
static double const pd_device[3][3] = {
    {1, 0, 0}, {0, 1, 0}, {.25, .5, .75}
};
static unsigned char const pd_wire[3][3] = {
    {255, 0, 0}, {0, 255, 0}, {64, 128, 191}
};
/* Independent chromaticity/transfer references; see the delta guide. */
static double const pd_work[5][3][3] = {
    { { 1.0930663624352, -0.22674197356975, -0.15013458093712 },
      { -0.51160498258534, 1.0182656579378, -0.31067462129058 },
      { 0.12407596556951, 0.50734576865241, 0.77112740734811 } },
    { { 1.2249401762806, -0.042056954709688, -0.019637554590334 },
      { -0.22494017628056, 1.0420569547097, -0.078636045550632 },
      { 0.014173712541967, 0.22090336569747, 0.55604119739664 } },
    { { 0.64857407194143, 0.26204175072605, 0.14500192827479 },
      { 0.8488292869841, -0.30424052175479, 0.20796743046686 },
      { 0.58353085687825, -0.057052070168363, -0.12120522630902 } },
    { { 0.54970952834149, 0.7351091454253, 0.74035572897474 },
      { 0.86588907613231, -0.90066633129596, 0.84112404854987 },
      { 0.51964299881485, -0.024190924241413, -0.33268467557514 } },
    { { 0.58723742460956, 0.40023305508709, 0.32314385132397 },
      { 0.88244968609601, -0.3801545974064, 0.38760984200729 },
      { 0.55771760713899, -0.16217401415405, -0.26830746085521 } },
};
static double const pd_negative[3] = {
    -0.0061247008814028, 0.00021028477354844, 9.8187772951672e-05
};
static double const pd_retained[3] = {
    0.0048036869658061, -0.00016492923415564, -7.7010018001311e-05
};

static int
pd_palette(sixel_dither_t **dither, int workspace)
{
    unsigned char bytes[9] = {0, 0, 0, 255, 255, 255, 0, 0, 0};
    float values[9] = {0, 0, 0, 1, 1, 1, 0, 0, 0};
    sixel_palette_float32_entries_request_t request;

    if (SIXEL_FAILED(sixel_dither_new(dither, 3, NULL))) {
        return 0;
    }
    sixel_dither_set_palette(*dither, bytes);
    memset(&request, 0, sizeof(request));
    request.entries = values;
    request.colors = 3;
    request.depth = 3 * sizeof(float);
    (*dither)->use_rgb101_palette = 1;
    (*dither)->force_palette = 1;
    sixel_dither_set_transparent(*dither, 2);
    sixel_dither_set_lut_policy(*dither, SIXEL_LUT_POLICY_NONE);
    if (workspace != SIXEL_COLORSPACE_GAMMA) {
        /* The residual fixture uses black and linear sRGB red. */
        values[4] = values[5] = 0.0f;
    }
    return SIXEL_SUCCEEDED((*dither)->palette->vtbl->init_entries_float32(
        (*dither)->palette, &request)) &&
        SIXEL_SUCCEEDED(sixel_palette_prepare_output((*dither)->palette,
            workspace, SIXEL_PALETTE_OUTPUT_DISPLAY_P3,
            SIXEL_COLORSPACE_GAMMA));
}

int
test_delta_p3_transform(int workspace)
{
    sixel_dither_t *dither;
    double result[3];
    int color;
    int channel;
    int ok;

    dither = NULL;
    ok = 0;
    if (!pd_palette(&dither, SIXEL_COLORSPACE_GAMMA)) {
        goto end;
    }
    for (color = 0; color < 3; ++color) {
        memcpy(result, pd_device[color], sizeof(result));
        if (!sixel_palette_from_output(dither->palette, result, result,
                pd_workspaces[workspace])) {
            goto end;
        }
        for (channel = 0; channel < 3; ++channel) {
            if (!isfinite(result[channel]) ||
                    fabs(result[channel] - pd_work[workspace][color][channel])
                        > 3e-6) {
                fprintf(stderr, "P3 inverse W=%d color=%d channel=%d: %.9g\n",
                    workspace, color, channel, result[channel]);
                goto end;
            }
        }
        /* Forward and reverse use separate independent literals. A shared
         * roundtrip cannot establish the primaries or transfer contract. */
        memcpy(result, pd_work[workspace][color], sizeof(result));
        if (!sixel_palette_to_output(dither->palette, result, result,
                pd_workspaces[workspace])) {
            goto end;
        }
        for (channel = 0; channel < 3; ++channel) {
            if (!isfinite(result[channel]) ||
                    fabs(result[channel] - pd_device[color][channel]) > 3e-6) {
                fprintf(stderr, "P3 forward W=%d color=%d channel=%d: %.9g\n",
                    workspace, color, channel, result[channel]);
                goto end;
            }
        }
    }
    ok = 1;
end:
    sixel_dither_unref(dither);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}

int
test_delta_p3_threshold(int unused)
{
    unsigned char history[3] = {1, 0, 0};
    unsigned char valid[1] = {1};
    float query[3];
    float chosen[3] = {0, 0, 0};
    float retained[3];
    sixel_dither_t *dither;
    int channel;
    int threshold;
    int keep;
    int ok;

    (void)unused;
    dither = NULL;
    ok = 0;
    if (!pd_palette(&dither, SIXEL_COLORSPACE_GAMMA)) {
        goto end;
    }
    for (channel = 0; channel < 3; ++channel) {
        query[channel] = (float)(1.34 * pd_negative[channel]);
    }
    for (threshold = 2; threshold <= 3; ++threshold) {
        sixel_dither_set_pipeline_accumulation_buffer_hint(dither,
            history, sizeof(history), valid, sizeof(valid),
            1, 1, 0, 0, 1, 1, 2, 1, (unsigned int)threshold,
            SIXEL_6DELTA_ERROR_DIFFUSE);
        keep = sixel_dither_6delta_keep_float(dither, 0, 0, 0,
            SIXEL_PIXELFORMAT_RGBFLOAT32, query, chosen, 0, 0, retained);
        /* P3 red distance .010621569 lies between 2/255 and 3/255.
         * The unconverted sRGB distance exceeds 3/255; clipping gives
         * a value below 2/255. Both errors change this keep decision. */
        if (keep != (threshold == 3)) {
            goto end;
        }
        for (channel = 0; keep && channel < 3; ++channel) {
            if (!isfinite(retained[channel]) ||
                    fabs(retained[channel] - pd_retained[channel]) > 2e-8) {
                goto end;
            }
        }
    }
    ok = 1;
end:
    sixel_dither_unref(dither);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}

int
test_delta_p3_residual(int skip)
{
    unsigned char history[6] = {255, 0, 0, 0, 0, 0};
    unsigned char valid[2] = {1, 0};
    float pixels[6] = {0, 0, 0, .6f, 0, 0};
    sixel_dither_t *dither;
    sixel_index_t *indexes;
    int channel;
    int ok;

    dither = NULL;
    indexes = NULL;
    ok = 0;
    if (!pd_palette(&dither, SIXEL_COLORSPACE_LINEAR)) {
        goto end;
    }
    for (channel = 0; channel < 3; ++channel) {
        pixels[channel] = (float)(.8 * pd_work[1][0][channel]);
    }
    sixel_dither_set_pipeline_accumulation_buffer_hint(dither,
        history, sizeof(history), valid, sizeof(valid),
        2, 1, 0, 0, 2, 1, 2, 1, 32,
        skip ? SIXEL_6DELTA_ERROR_SKIP : SIXEL_6DELTA_ERROR_DIFFUSE);
    sixel_dither_set_pixelformat(dither, SIXEL_PIXELFORMAT_LINEARRGBFLOAT32);
    sixel_dither_set_diffusion_type(dither, SIXEL_DIFFUSE_FS);
    sixel_dither_set_diffusion_scan(dither, SIXEL_SCAN_RASTER);
    if (SIXEL_FAILED(sixel_dither_set_clamp_policy(dither,
            SIXEL_DITHER_CLAMP_NONE, 1.01))) {
        goto end;
    }
    indexes = sixel_dither_apply_palette(dither,
        (unsigned char *)pixels, 2, 1);
    /* P3 red is linear sRGB (1.224940176,-.042056955,-.019637555).
     * FS adds -.2*1.224940176*7/16 to the next red .6: .492817735,
     * below the black/red boundary .5. Raw P3 residual or clipping the
     * retained linear red to 1 incorrectly selects red. Skip selects red. */
    ok = indexes != NULL && indexes[0] == 2 && indexes[1] == (skip ? 1 : 0);
end:
    if (dither != NULL) {
        sixel_allocator_free(dither->allocator, indexes);
    }
    sixel_dither_unref(dither);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}

int
test_delta_p3_cache(int failure)
{
    sixel_dither_t *dither;
    sixel_dither_t *other;
    sixel_palette_float32_entries_request_t empty;
    unsigned char const *codes;
    unsigned char saved[9];
    unsigned int count;
    double invalid[3] = {NAN, 0, 0};
    double output[3] = {7, 8, 9};
    double sentinel[3] = {7, 8, 9};
    int ok;

    dither = other = NULL;
    ok = 0;
    if (!pd_palette(&dither, SIXEL_COLORSPACE_GAMMA) ||
            !pd_palette(&other, SIXEL_COLORSPACE_GAMMA)) {
        goto end;
    }
    if (failure) {
        if (SIXEL_FAILED(sixel_palette_get_rgb101(dither->palette,
                &codes, &count)) || count != 3) {
            goto end;
        }
        memcpy(saved, codes, sizeof(saved));
        ok = !sixel_palette_to_output(dither->palette, output, invalid, 0) &&
            memcmp(output, sentinel, sizeof(output)) == 0 &&
            !sixel_palette_from_output(dither->palette, output, invalid, 0) &&
            memcmp(output, sentinel, sizeof(output)) == 0 &&
            SIXEL_FAILED(sixel_palette_prepare_output(dither->palette, 0,
                SIXEL_PALETTE_OUTPUT_DISPLAY_P3, SIXEL_COLORSPACE_LINEAR)) &&
            SIXEL_SUCCEEDED(sixel_palette_get_rgb101(dither->palette,
                &codes, &count)) && count == 3 &&
            memcmp(saved, codes, sizeof(saved)) == 0 &&
            sixel_palette_output_is_p3(dither->palette);
        goto end;
    }
    if (SIXEL_FAILED(sixel_palette_prepare_output(dither->palette, 0,
            SIXEL_PALETTE_OUTPUT_LEGACY, SIXEL_COLORSPACE_GAMMA)) ||
            sixel_palette_output_is_p3(dither->palette) ||
            !sixel_palette_output_is_p3(other->palette) ||
            !sixel_palette_from_output(dither->palette, output,
                pd_device[0], 0) || fabs(output[0] - 1.0) > 1e-12 ||
            fabs(output[1]) > 1e-12 || fabs(output[2]) > 1e-12) {
        goto end;
    }
    memset(&empty, 0, sizeof(empty));
    if (SIXEL_FAILED(other->palette->vtbl->init_entries_float32(
            other->palette, &empty))) {
        goto end;
    }
    ok = !sixel_palette_output_is_p3(other->palette) &&
        !sixel_palette_to_output(other->palette, output, pd_device[0], 0) &&
        !sixel_palette_from_output(other->palette, output, pd_device[0], 0);
end:
    sixel_dither_unref(dither);
    sixel_dither_unref(other);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}

typedef struct pd_stream {
    unsigned char data[65536];
    size_t size;
} pd_stream_t;

static int
pd_write(char *data, int size, void *context)
{
    pd_stream_t *stream;

    stream = context;
    if (size < 0 || (size_t)size > sizeof(stream->data) - stream->size) {
        return -1;
    }
    memcpy(stream->data + stream->size, data, (size_t)size);
    stream->size += (size_t)size;
    return size;
}

static int
pd_encoder(sixel_encoder_t **encoder, sixel_output_t **output,
           pd_stream_t *stream, int fixed)
{
    static struct { int flag; char const *value; } const options[] = {
        {'=', "1"}, {'~', "none"}, {'d', "none"}, {'Q', "heckbert"},
        {'F', "none"}, {'a', "off"}, {'_', "none"}, {'0', "none"},
        {'X', "gamma"}, {'W', "gamma"}, {'U', "display-p3"}, {'t', "rgb"},
        {'.', "float32"}, {'p', "4"}, {'4', "full-frame"}, {'5', "none"},
        {'Z', "delta"}, {'7', NULL}
    };
    int i;

    if (SIXEL_FAILED(sixel_encoder_new(encoder, NULL)) ||
            SIXEL_FAILED(sixel_output_new(output, pd_write, stream, NULL))) {
        return 0;
    }
    for (i = 0; i < (int)(sizeof(options) / sizeof(options[0])); ++i) {
        if (fixed && (options[i].flag == 'Q' || options[i].flag == 'F' ||
                options[i].flag == 'a' || options[i].flag == 'X' ||
                options[i].flag == 'p' || options[i].flag == '4' ||
                options[i].flag == '5')) {
            continue;
        }
        if (SIXEL_FAILED(sixel_encoder_setopt(*encoder, options[i].flag,
                options[i].value))) {
            return 0;
        }
    }
    return 1;
}

/* Decode the actual wire and check every paint bit and painted triplet.
 * Expectations are independent output codes, not the encoder's palette view. */
static int
pd_decode(sixel_encoder_t *encoder, pd_stream_t *stream,
          int width, int height, int keep, unsigned char const color[3])
{
    unsigned char *indexes;
    unsigned char *mask;
    unsigned char *palette;
    unsigned int flags;
    int w;
    int h;
    int count;
    int i;
    int ok;

    indexes = mask = palette = NULL;
    ok = 0;
    if (SIXEL_FAILED(sixel_decode_raw_with_options_mask(stream->data,
            (int)stream->size, 0, &indexes, &mask, &w, &h, &palette, &count,
            &flags, encoder->allocator)) || w != width || h != height ||
            mask == NULL) {
        goto end;
    }
    for (i = 0; i < w * h; ++i) {
        if ((mask[i] != 0) == keep || (!keep &&
                memcmp(palette + indexes[i] * 3, color, 3) != 0)) {
            fprintf(stderr, "delta pixel=%d mask=%d keep=%d "
                "got=%d/%d/%d want=%d/%d/%d\n", i, mask[i], keep,
                palette[indexes[i]*3], palette[indexes[i]*3+1],
                palette[indexes[i]*3+2], color[0], color[1], color[2]);
            goto end;
        }
    }
    ok = 1;
end:
    sixel_allocator_free(encoder->allocator, indexes);
    sixel_allocator_free(encoder->allocator, mask);
    sixel_allocator_free(encoder->allocator, palette);
    return ok;
}

int
test_delta_p3_frames(int workspace, int workers)
{
    sixel_encoder_t *encoder;
    sixel_output_t *output;
    sixel_frame_t *frame;
    sixel_dither_t *dither;
    pd_stream_t stream;
    float pixels[4 * 25 * 3];
    int rows;
    int pass;
    int color;
    int i;
    int ok;

    encoder = NULL;
    output = NULL;
    frame = NULL;
    ok = 0;
    rows = workers ? 25 : 1;
    if (!pd_encoder(&encoder, &output, &stream, 0) ||
            SIXEL_FAILED(sixel_encoder_setopt(encoder, 'W',
                pd_spaces[workspace])) ||
            SIXEL_FAILED(sixel_encoder_setopt(encoder, '=',
                workers ? "4" : "1"))) {
        goto end;
    }
    for (pass = 0; pass < 7; ++pass) {
        color = pass == 6 ? 2 : pass / 2;
        for (i = 0; i < 4 * rows * 3; ++i) {
            pixels[i] = (float)pd_work[0][color][i % 3];
        }
        if (pass == 6) {
            sixel_encoder_invalidate_6delta_plane(encoder);
        }
        stream.size = 0;
        if (SIXEL_FAILED(sixel_frame_new(&frame, NULL)) ||
                SIXEL_FAILED(sixel_frame_init_borrowed(frame,
                    (unsigned char *)pixels, 4, rows,
                    SIXEL_PIXELFORMAT_RGBFLOAT32, NULL, -1)) ||
                SIXEL_FAILED(sixel_encoder_encode_frame(encoder, frame,
                    output))) {
            goto end;
        }
        dither = encoder->diagnostic_dither;
        if (dither == NULL || !dither->use_rgb101_palette ||
                dither->pixelformat != pd_formats[workspace] ||
                !sixel_palette_output_is_p3(dither->palette) ||
                (workers && dither->pipeline_last_encode_threads < 2) ||
                !pd_decode(encoder, &stream, 4, rows, pass % 2,
                    pd_wire[color])) {
            goto end;
        }
        for (i = 0; i < 4 * rows * 3; ++i) {
            if (encoder->accumulation_pixels[i] != pd_wire[color][i % 3]) {
                goto end;
            }
        }
        sixel_frame_unref(frame);
        frame = NULL;
    }
    ok = 1;
end:
    if (!ok) {
        fprintf(stderr, "P3 frames workspace=%d workers=%d\n", workspace,
            workers);
    }
    sixel_frame_unref(frame);
    sixel_output_unref(output);
    sixel_encoder_unref(encoder);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}

/* Black has identical numerical coordinates in both definitions. A stale
 * interpretation cannot be detected just by comparing different primaries.
 * Switching definitions must repaint even this otherwise identical frame. */
int
test_delta_p3_identity(int seeded)
{
    static char const *const definitions[] = {
        "display-p3", "display-p3", "gamma", "gamma", "display-p3",
        "display-p3", "display-p3"
    };
    unsigned char black[12] = {0};
    float pixels[12] = {0};
    sixel_encoder_t *encoder;
    sixel_output_t *output;
    sixel_frame_t *frame;
    pd_stream_t stream;
    int keep;
    int pass;
    int ok;

    encoder = NULL;
    output = NULL;
    frame = NULL;
    ok = 0;
    pass = -1;
    if (!pd_encoder(&encoder, &output, &stream, 1) ||
            SIXEL_FAILED(sixel_encoder_setopt(encoder, 'b', "gray1"))) {
        goto end;
    }
    if (seeded && SIXEL_FAILED(sixel_encoder_set_accumulation_buffer(
            encoder, black, 4, 1, SIXEL_PIXELFORMAT_RGB888))) {
        goto end;
    }
    for (pass = 0; pass < 7; ++pass) {
        if (SIXEL_FAILED(sixel_encoder_setopt(encoder, 'U',
                definitions[pass]))) {
            goto end;
        }
        if (pass == 5 && SIXEL_SUCCEEDED(sixel_encoder_setopt(encoder,
                'U', "not-a-colorspace"))) {
            goto end;
        }
        if (pass == 6 && SIXEL_FAILED(sixel_encoder_setopt(encoder,
                'W', "linear"))) {
            goto end;
        }
        memset(pixels, 0, sizeof(pixels));
        stream.size = 0;
        if (SIXEL_FAILED(sixel_frame_new(&frame, NULL)) ||
                SIXEL_FAILED(sixel_frame_init_borrowed(frame,
                    (unsigned char *)pixels, 4, 1,
                    SIXEL_PIXELFORMAT_RGBFLOAT32, NULL, -1)) ||
                SIXEL_FAILED(sixel_encoder_encode_frame(encoder, frame,
                    output))) {
            goto end;
        }
        keep = pass == 0 ? seeded : pass == 1 || pass == 3 || pass >= 5;
        if (!pd_decode(encoder, &stream, 4, 1, keep, black)) {
            goto end;
        }
        sixel_frame_unref(frame);
        frame = NULL;
    }
    ok = 1;
end:
    if (!ok) {
        fprintf(stderr, "P3 identity seeded=%d pass=%d: %s\n", seeded,
            pass, sixel_helper_get_additional_message());
    }
    sixel_frame_unref(frame);
    sixel_output_unref(output);
    sixel_encoder_unref(encoder);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}

/* Decimal inverse of the documented legacy output matrix and TRC. */
static double const od_input[3][3] = {
    { -0.25, 1.25, 0.5 },
    { 0, 1, 0 },
    { 0.501960784313725, 0, 0 },
};
static double const od_linear[2][3][3] = {
    { { -0.25, 1.25, 0.5 },
      { 0, 1, 0 },
      { 0.501960784313725, 0, 0 } },
    { { 0.0397109517371331, 1.58062160921521, 0.211826931114147 },
      { 0.0501803604257027, 0.965770562665531, -0.00436969856687614 },
      { 0.206283743391989, 0.00390204259140148, -0.000356045000672537 } },
};
static double const od_negative[2][3] = {
    { -0.0760540039746224, 0, 0 },
    { -0.000200271221733843, -3.78831033501505e-06, 3.45667409871548e-07 },
};
static double const od_retained[2][3] = {
    { 0.0498400898450127, 0, 0 },
    { 6.16404193025565e-05, 1.16598398650042e-06, -1.06391142467926e-07 },
};

static char const *const od_names[2] = {"linear", "smpte-c"};
static int const od_spaces[2] = {
    SIXEL_COLORSPACE_LINEAR, SIXEL_COLORSPACE_SMPTEC
};

/* Independent destination coordinates above include signed, above-one and
 * bounded values whose inverse leaves the sRGB cube. This is not a forward
 * / inverse roundtrip oracle. The legacy matrix itself is the contract. */
int
test_delta_output_inverse(int output)
{
    sixel_dither_t *dither;
    double result[3];
    int color;
    int channel;
    int ok;

    dither = NULL;
    ok = 0;
    if (!pd_palette(&dither, SIXEL_COLORSPACE_GAMMA) ||
            SIXEL_FAILED(sixel_palette_prepare_output(dither->palette, 0,
                SIXEL_PALETTE_OUTPUT_LEGACY, od_spaces[output]))) {
        goto end;
    }
    for (color = 0; color < 3; ++color) {
        memcpy(result, od_input[color], sizeof(result));
        if (!sixel_palette_from_output(dither->palette, result, result,
                SIXEL_COLORSPACE_LINEAR)) {
            goto end;
        }
        for (channel = 0; channel < 3; ++channel) {
            if (!isfinite(result[channel]) ||
                    fabs(result[channel] - od_linear[output][color][channel])
                        > 2e-12) {
                goto end;
            }
        }
    }
    ok = 1;
end:
    sixel_dither_unref(dither);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}

int
test_delta_output_failure(void)
{
    /* VMS exposes NAN and INFINITY through runtime values, so these
     * specimens must not require constant-expression initialization. */
    double const invalid[3][3] = {
        {NAN, 0, 0}, {0, INFINITY, 0}, {1e300, 0, 0}
    };
    double const sentinel[3] = {7, 8, 9};
    double result[3];
    int i;

    for (i = 0; i < 3; ++i) {
        memcpy(result, sentinel, sizeof(result));
        if (sixel_color_from_output(result, invalid[i],
                SIXEL_COLORSPACE_SMPTEC, SIXEL_COLORSPACE_LINEAR) ||
                memcmp(result, sentinel, sizeof(result)) != 0) {
            return EXIT_FAILURE;
        }
    }
    /* This inverse must not make SMPTE-C a valid workspace, or admit
     * working-space values as a wire interpretation. */
    if (sixel_color_from_output(result, od_input[1],
            SIXEL_COLORSPACE_SMPTEC, SIXEL_COLORSPACE_SMPTEC) ||
            sixel_color_from_output(result, od_input[1],
                SIXEL_COLORSPACE_OKLAB, SIXEL_COLORSPACE_GAMMA) ||
            memcmp(result, sentinel, sizeof(result)) != 0) {
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

int
test_delta_output_threshold(int output)
{
    unsigned char history[3] = {1, 0, 0};
    unsigned char valid[1] = {1};
    float query[3];
    float chosen[3] = {0};
    float retained[3];
    sixel_dither_t *dither;
    int channel;
    int threshold;
    int keep;
    int ok;

    dither = NULL;
    ok = 0;
    if (!pd_palette(&dither, SIXEL_COLORSPACE_GAMMA) ||
            SIXEL_FAILED(sixel_palette_prepare_output(dither->palette, 0,
                SIXEL_PALETTE_OUTPUT_LEGACY, od_spaces[output]))) {
        goto end;
    }
    for (channel = 0; channel < 3; ++channel) {
        query[channel] = (float)od_negative[output][channel];
    }
    for (threshold = 2; threshold <= 3; ++threshold) {
        sixel_dither_set_pipeline_accumulation_buffer_hint(dither,
            history, sizeof(history), valid, sizeof(valid),
            1, 1, 0, 0, 1, 1, 2, 1, (unsigned int)threshold,
            SIXEL_6DELTA_ERROR_DIFFUSE);
        keep = sixel_dither_6delta_keep_float(dither, 0, 0, 0,
            SIXEL_PIXELFORMAT_RGBFLOAT32, query, chosen, 0, 0, retained);
        /* The output distance .0067+1/255 lies between T2 and T3.
         * Gamma comparison or premature clipping changes this decision. */
        if (keep != (threshold == 3)) {
            goto end;
        }
        for (channel = 0; keep && channel < 3; ++channel) {
            if (!isfinite(retained[channel]) || fabs(retained[channel] -
                    od_retained[output][channel]) > 2e-8) {
                goto end;
            }
        }
    }
    ok = 1;
end:
    sixel_dither_unref(dither);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}

int
test_delta_output_residual(int output, int skip)
{
    unsigned char history[6] = {128, 0, 0, 0, 0, 0};
    unsigned char valid[2] = {1, 0};
    float values[9] = {0, 0, 0, 1, 0, 0, 0, 0, 0};
    float pixels[6] = {0};
    sixel_palette_float32_entries_request_t request;
    sixel_dither_t *dither;
    sixel_index_t *indexes;
    int ok;

    dither = NULL;
    indexes = NULL;
    ok = 0;
    if (!pd_palette(&dither, SIXEL_COLORSPACE_GAMMA)) {
        goto end;
    }
    memset(&request, 0, sizeof(request));
    request.entries = values;
    request.colors = 3;
    request.depth = 3 * sizeof(float);
    if (SIXEL_FAILED(dither->palette->vtbl->init_entries_float32(
            dither->palette, &request)) ||
            SIXEL_FAILED(sixel_palette_prepare_output(dither->palette, 0,
                SIXEL_PALETTE_OUTPUT_LEGACY, od_spaces[output]))) {
        goto end;
    }
    pixels[0] = output ? .45f : .65f;
    pixels[3] = output ? .516f : .53f;
    sixel_dither_set_pipeline_accumulation_buffer_hint(dither,
        history, sizeof(history), valid, sizeof(valid),
        2, 1, 0, 0, 2, 1, 2, 1, 64,
        skip ? SIXEL_6DELTA_ERROR_SKIP : SIXEL_6DELTA_ERROR_DIFFUSE);
    sixel_dither_set_pixelformat(dither, SIXEL_PIXELFORMAT_RGBFLOAT32);
    sixel_dither_set_diffusion_type(dither, SIXEL_DIFFUSE_FS);
    sixel_dither_set_diffusion_scan(dither, SIXEL_SCAN_RASTER);
    if (SIXEL_FAILED(sixel_dither_set_clamp_policy(dither,
            SIXEL_DITHER_CLAMP_NONE, 1.01))) {
        goto end;
    }
    indexes = sixel_dither_apply_palette(dither,
        (unsigned char *)pixels, 2, 1);
    /* Retained red in gamma units: .73664694 linear / .49152858 SMPTE-C.
     * FS moves the next red to .49209196 / .49783125, below .5. Skip leaves
     * it above .5. Green/blue residuals cancel in the black/red comparison. */
    ok = indexes != NULL && indexes[0] == 2 && indexes[1] == (skip ? 1 : 0);
end:
    if (dither != NULL) {
        sixel_allocator_free(dither->allocator, indexes);
    }
    sixel_dither_unref(dither);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}

int
test_delta_output_frames(int space, int target, int workers)
{
    sixel_encoder_t *encoder;
    sixel_output_t *output;
    sixel_frame_t *frame;
    sixel_dither_t *dither;
    pd_stream_t stream;
    float pixels[4 * 25 * 3];
    unsigned char expected[3];
    int rows;
    int pass;
    int color;
    int i;
    int ok;

    encoder = NULL;
    output = NULL;
    frame = NULL;
    ok = 0;
    rows = workers ? 25 : 1;
    if (!pd_encoder(&encoder, &output, &stream, 0) ||
            SIXEL_FAILED(sixel_encoder_setopt(encoder, 'U',
                od_names[target])) ||
            SIXEL_FAILED(sixel_encoder_setopt(encoder, 'W',
                pd_spaces[space])) ||
            SIXEL_FAILED(sixel_encoder_setopt(encoder, '=',
                workers ? "4" : "1"))) {
        goto end;
    }
    for (pass = 0; pass < 5; ++pass) {
        color = pass == 4 ? 1 : pass / 2;
        for (i = 0; i < 4 * rows * 3; ++i) {
            pixels[i] = (float)output_native[0].entries[color][i % 3];
        }
        for (i = 0; i < 3; ++i) {
            expected[i] = (unsigned char)((255U *
                output_native[0].wire[target][color][i] + 50U) / 100U);
        }
        if (pass == 4) {
            sixel_encoder_invalidate_6delta_plane(encoder);
        }
        stream.size = 0;
        if (SIXEL_FAILED(sixel_frame_new(&frame, NULL)) ||
                SIXEL_FAILED(sixel_frame_init_borrowed(frame,
                    (unsigned char *)pixels, 4, rows,
                    SIXEL_PIXELFORMAT_RGBFLOAT32, NULL, -1)) ||
                SIXEL_FAILED(sixel_encoder_encode_frame(encoder, frame,
                    output))) {
            goto end;
        }
        dither = encoder->diagnostic_dither;
        if (dither == NULL || !dither->use_rgb101_palette ||
                dither->pixelformat != pd_formats[space] ||
                (workers && dither->pipeline_last_encode_threads < 2) ||
                !pd_decode(encoder, &stream, 4, rows, pass % 2, expected)) {
            goto end;
        }
        for (i = 0; i < 4 * rows * 3; ++i) {
            if (encoder->accumulation_pixels[i] != expected[i % 3]) {
                goto end;
            }
        }
        sixel_frame_unref(frame);
        frame = NULL;
    }
    ok = 1;
end:
    if (!ok) {
        fprintf(stderr, "output delta space=%d target=%d workers=%d\n",
            space, target, workers);
    }
    sixel_frame_unref(frame);
    sixel_output_unref(output);
    sixel_encoder_unref(encoder);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}

int
test_delta_output_identity(int target)
{
    unsigned char black[12] = {0};
    float pixels[12] = {0};
    sixel_encoder_t *encoder;
    sixel_output_t *output;
    sixel_frame_t *frame;
    pd_stream_t stream;
    char const *definition;
    int pass;
    int ok;

    encoder = NULL;
    output = NULL;
    frame = NULL;
    ok = 0;
    if (!pd_encoder(&encoder, &output, &stream, 1) ||
            SIXEL_FAILED(sixel_encoder_setopt(encoder, 'U',
                od_names[target])) ||
            SIXEL_FAILED(sixel_encoder_setopt(encoder, 'b', "gray1")) ||
            SIXEL_FAILED(sixel_encoder_set_accumulation_buffer(encoder,
                black, 4, 1, SIXEL_PIXELFORMAT_RGB888))) {
        goto end;
    }
    for (pass = 0; pass < 8; ++pass) {
        definition = pass < 2 ? od_names[target] :
            pass < 4 ? od_names[1 - target] : pass < 6 ? "gamma" : "display-p3";
        if (SIXEL_FAILED(sixel_encoder_setopt(encoder, 'U', definition)) ||
                (pass == 1 && SIXEL_FAILED(sixel_encoder_setopt(encoder,
                    'W', "linear")))) {
            goto end;
        }
        memset(pixels, 0, sizeof(pixels));
        stream.size = 0;
        if (SIXEL_FAILED(sixel_frame_new(&frame, NULL)) ||
                SIXEL_FAILED(sixel_frame_init_borrowed(frame,
                    (unsigned char *)pixels, 4, 1,
                    SIXEL_PIXELFORMAT_RGBFLOAT32, NULL, -1)) ||
                SIXEL_FAILED(sixel_encoder_encode_frame(encoder, frame,
                    output)) ||
                !pd_decode(encoder, &stream, 4, 1,
                    pass == 0 || pass % 2, black)) {
            goto end;
        }
        sixel_frame_unref(frame);
        frame = NULL;
    }
    ok = 1;
end:
    sixel_frame_unref(frame);
    sixel_output_unref(output);
    sixel_encoder_unref(encoder);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}

/* vim: set ts=4 sts=4 sw=4 et: */

/* Decimal references use independently derived primaries/TRCs and a
 * triangular-wave HSL inverse. Field margins exceed .005, except exact
 * branches. RGB101 and history bytes are discrete expectations. */
static int const hd_fields[4][2][3] = {
    { { 332, 46, 70 }, { 97, 47, 52 } },
    { { 344, 29, 94 }, { 106, 25, 84 } },
    { { 329, 42, 83 }, { 96, 46, 56 } },
    { { 337, 49, 54 }, { 98, 46, 43 } },
};
static unsigned char const hd_colors[4][2][3] = {
    { { 36, 112, 199 }, { 181, 59, 105 } },
    { { 5, 41, 143 }, { 117, 10, 36 } },
    { { 18, 110, 196 }, { 184, 51, 105 } },
    { { 59, 110, 191 }, { 168, 66, 105 } },
};
static float const hd_frame_samples[2][3] = {
    {.137f, .437f, .781f}, {.71f, .223f, .41f}
};
static char const *const hd_outputs[4] = {
    "gamma", "linear", "smpte-c", "display-p3"
};
static float const hd_samples[2][3] = {
    {.123f, .456f, .789f}, {.7f, .2f, .4f}
};

static int
hd_palette(sixel_dither_t **dither)
{
    float values[9] = {.123f, .456f, .789f, .7f, .2f, .4f, 0, 0, 0};
    sixel_palette_float32_entries_request_t request;

    if (!pd_palette(dither, SIXEL_COLORSPACE_GAMMA)) {
        return 0;
    }
    memset(&request, 0, sizeof(request));
    request.entries = values;
    request.colors = 3;
    request.depth = 3 * sizeof(float);
    return SIXEL_SUCCEEDED((*dither)->palette->vtbl->init_entries_float32(
        (*dither)->palette, &request)) &&
        SIXEL_SUCCEEDED(sixel_palette_prepare_output((*dither)->palette,
            SIXEL_COLORSPACE_GAMMA, SIXEL_PALETTE_OUTPUT_LEGACY,
            SIXEL_COLORSPACE_GAMMA));
}

int
test_delta_hls_cache(int mode)
{
    unsigned char const *codes;
    unsigned char const *other;
    unsigned char const expected[9] = {12, 46, 80, 70, 20, 40, 0, 0, 0};
    unsigned char bytes[9] = {0};
    float values[9] = {0};
    sixel_palette_float32_entries_request_t request;
    sixel_dither_t *dither;
    sixel_dither_t *second;
    unsigned int count;
    int ok;

    dither = second = NULL;
    ok = 0;
    if (!hd_palette(&dither) ||
            SIXEL_SUCCEEDED(sixel_palette_get_hls_rgb101(dither->palette,
                &codes, &count)) || codes != NULL || count != 0 ||
            SIXEL_FAILED(sixel_palette_prepare_hls(dither->palette)) ||
            SIXEL_FAILED(sixel_palette_get_hls_rgb101(dither->palette,
                &codes, &count)) || count != 3 ||
            memcmp(codes, expected, sizeof(expected)) != 0) {
        goto end;
    }
    if (mode == 0) {
        /* Independently prepared palettes cannot overwrite the borrowed view. */
        if (!pd_palette(&second, SIXEL_COLORSPACE_GAMMA) ||
                SIXEL_FAILED(sixel_palette_prepare_hls(second->palette)) ||
                SIXEL_FAILED(sixel_palette_get_hls_rgb101(second->palette,
                    &other, &count)) || codes == other || other[3] != 100 ||
                memcmp(codes, expected, sizeof(expected)) != 0) {
            goto end;
        }
    } else {
        /* Reinitialization invalidates all derived output interpretations. */
        request.entries = values;
        request.colors = 3;
        request.depth = 3 * sizeof(float);
        if (SIXEL_FAILED(dither->palette->vtbl->init_entries_float32(
                dither->palette, &request))) {
            goto end;
        }
        if (SIXEL_SUCCEEDED(sixel_palette_prepare_hls(dither->palette)) ||
                SIXEL_SUCCEEDED(sixel_palette_get_hls_rgb101(dither->palette,
                    &codes, &count)) || codes != NULL || count != 0 ||
                SIXEL_FAILED(sixel_palette_prepare_output(dither->palette,
                    SIXEL_COLORSPACE_GAMMA, SIXEL_PALETTE_OUTPUT_LEGACY,
                    SIXEL_COLORSPACE_GAMMA)) ||
                SIXEL_FAILED(sixel_palette_prepare_hls(dither->palette)) ||
                SIXEL_FAILED(sixel_palette_get_hls_rgb101(dither->palette,
                    &codes, &count)) || count != 3 ||
                memcmp(codes, bytes, sizeof(bytes)) != 0) {
            goto end;
        }
    }
    ok = 1;
end:
    sixel_dither_unref(dither);
    sixel_dither_unref(second);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}

int
test_delta_hls_candidate(int mode)
{
    unsigned char history[3] = {31, 117, 204};
    unsigned char valid[1] = {1};
    float query[3] = {.123f, .456f, .789f};
    float retained[3];
    sixel_dither_t *dither;
    int hls;
    int channel;
    int keep;
    int ok;

    dither = NULL;
    ok = 0;
    if (!hd_palette(&dither) ||
            SIXEL_FAILED(sixel_palette_prepare_hls(dither->palette))) {
        goto end;
    }
    if (mode) {
        history[2] = 201;
        query[0] = .12f;
        query[1] = .46f;
        query[2] = .80f;
    }
    sixel_dither_set_pipeline_accumulation_buffer_hint(dither,
        history, sizeof(history), valid, sizeof(valid),
        1, 1, 0, 0, 1, 1, 2, 1, 0, SIXEL_6DELTA_ERROR_DIFFUSE);
    for (hls = 0; hls < 2; ++hls) {
        dither->pipeline_6delta_hls = hls;
        keep = sixel_dither_6delta_keep_float(dither, 0, 0, 0,
            SIXEL_PIXELFORMAT_RGBFLOAT32, query, hd_samples[0], 0, 0,
            retained);
        /* Same centroid: RGB paints (31,117,201), HLS paints (31,117,204).
         * The first case keeps only HLS; the second repaints only HLS.
         * The source remains unrounded in both distance comparisons. */
        if (keep != (mode ? !hls : hls)) {
            goto end;
        }
        for (channel = 0; keep && channel < 3; ++channel) {
            if (!isfinite(retained[channel]) ||
                    fabs(retained[channel] - history[channel] / 255.0)
                        > 3e-8) {
                goto end;
            }
        }
    }
    ok = 1;
end:
    sixel_dither_unref(dither);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}

int
test_delta_hls_frames(int target, int workspace, int workers)
{
    sixel_encoder_t *encoder;
    sixel_output_t *output;
    sixel_frame_t *frame;
    sixel_dither_t *dither;
    pd_stream_t stream;
    float pixels[4 * 25 * 3];
    char field[64];
    int rows;
    int pass;
    int color;
    int i;
    int ok;

    encoder = NULL;
    output = NULL;
    frame = NULL;
    ok = 0;
    pass = -1;
    rows = workers ? 25 : 1;
    if (!pd_encoder(&encoder, &output, &stream, 0) ||
            SIXEL_FAILED(sixel_encoder_setopt(encoder, 'U',
                hd_outputs[target])) ||
            SIXEL_FAILED(sixel_encoder_setopt(encoder, 'W',
                pd_spaces[workspace])) ||
            SIXEL_FAILED(sixel_encoder_setopt(encoder, 't', "hls")) ||
            SIXEL_FAILED(sixel_encoder_setopt(encoder, '=',
                workers ? "4" : "1"))) {
        goto end;
    }
    for (pass = 0; pass < 5; ++pass) {
        color = pass < 2 ? 0 : 1;
        for (i = 0; i < 4 * rows * 3; ++i) {
            pixels[i] = hd_frame_samples[color][i % 3];
        }
        if (pass == 4) {
            sixel_encoder_invalidate_6delta_plane(encoder);
        }
        stream.size = 0;
        if (SIXEL_FAILED(sixel_frame_new(&frame, NULL)) ||
                SIXEL_FAILED(sixel_frame_init_borrowed(frame,
                    (unsigned char *)pixels, 4, rows,
                    SIXEL_PIXELFORMAT_RGBFLOAT32, NULL, -1)) ||
                SIXEL_FAILED(sixel_encoder_encode_frame(encoder, frame,
                    output))) {
            goto end;
        }
        dither = encoder->diagnostic_dither;
        stream.data[stream.size] = '\0';
        snprintf(field, sizeof(field), ";1;%d;%d;%d",
            hd_fields[target][color][0], hd_fields[target][color][1],
            hd_fields[target][color][2]);
        if (dither == NULL || !dither->use_rgb101_palette ||
                dither->pipeline_6delta_hls != 0 ||
                dither->pixelformat != pd_formats[workspace] ||
                (workers && dither->pipeline_last_encode_threads < 2) ||
                (!(pass % 2) &&
                 strstr((char *)stream.data, field) == NULL) ||
                !pd_decode(encoder, &stream, 4, rows, pass % 2,
                    hd_colors[target][color])) {
            fprintf(stderr, "wire: %s expected %s\n", stream.data, field);
            goto end;
        }
        for (i = 0; i < 4 * rows * 3; ++i) {
            if (encoder->accumulation_pixels[i] !=
                    hd_colors[target][color][i % 3]) {
                fprintf(stderr, "history %d=%d want=%d\n", i,
                    encoder->accumulation_pixels[i],
                    hd_colors[target][color][i % 3]);
                goto end;
            }
        }
        sixel_frame_unref(frame);
        frame = NULL;
    }
    ok = 1;
end:
    if (!ok) {
        fprintf(stderr, "HLS target=%d W=%d workers=%d pass=%d: %s\n",
            target, workspace, workers, pass,
            sixel_helper_get_additional_message());
    }
    sixel_frame_unref(frame);
    sixel_output_unref(output);
    sixel_encoder_unref(encoder);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}

int
test_delta_hls_reuse(void)
{
    unsigned char const rgb[3] = {31, 117, 201};
    unsigned char const hls[3] = {31, 117, 204};
    sixel_encoder_t *encoder;
    sixel_output_t *output;
    sixel_frame_t *frame;
    pd_stream_t stream;
    float pixels[3];
    unsigned char const *expected;
    int pass;
    int keep;
    int ok;

    encoder = NULL;
    output = NULL;
    frame = NULL;
    ok = 0;
    if (!pd_encoder(&encoder, &output, &stream, 0) ||
            SIXEL_FAILED(sixel_encoder_setopt(encoder, 'U', "gamma"))) {
        goto end;
    }
    for (pass = 0; pass < 5; ++pass) {
        if (SIXEL_FAILED(sixel_encoder_setopt(encoder, 't',
                pass == 2 || pass == 3 ? "rgb" : "hls"))) {
            goto end;
        }
        memcpy(pixels, hd_samples[0], sizeof(pixels));
        stream.size = 0;
        if (SIXEL_FAILED(sixel_frame_new(&frame, NULL)) ||
                SIXEL_FAILED(sixel_frame_init_borrowed(frame,
                    (unsigned char *)pixels, 1, 1,
                    SIXEL_PIXELFORMAT_RGBFLOAT32, NULL, -1)) ||
                SIXEL_FAILED(sixel_encoder_encode_frame(encoder, frame,
                    output))) {
            goto end;
        }
        /* Switching syntax preserves compatible history. RGB repaints the
         * closer blue 201; later HLS keeps it instead of repainting 204. */
        keep = pass == 1 || pass >= 3;
        expected = pass < 2 ? hls : rgb;
        if (!pd_decode(encoder, &stream, 1, 1, keep, expected) ||
                memcmp(encoder->accumulation_pixels, expected, 3) != 0) {
            goto end;
        }
        sixel_frame_unref(frame);
        frame = NULL;
    }
    ok = 1;
end:
    sixel_frame_unref(frame);
    sixel_output_unref(output);
    sixel_encoder_unref(encoder);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}

int
test_delta_hls_failure(void)
{
    unsigned char history[3] = {31, 117, 204};
    unsigned char valid[1] = {1};
    float pixels[3];
    sixel_dither_t *dither;
    sixel_output_t *output;
    pd_stream_t stream;
    SIXELSTATUS status;
    int pass;
    int ok;

    dither = NULL;
    output = NULL;
    ok = 0;
    if (!hd_palette(&dither) ||
            SIXEL_FAILED(sixel_output_new(&output, pd_write, &stream, NULL))) {
        goto end;
    }
    sixel_output_set_palette_type(output, SIXEL_PALETTETYPE_HLS);
    sixel_dither_set_diffusion_type(dither, SIXEL_DIFFUSE_NONE);
    for (pass = 0; pass < 2; ++pass) {
        stream.size = 0;
        memcpy(pixels, hd_samples[0], sizeof(pixels));
        sixel_dither_set_pixelformat(dither, pass == 0 ? 0x7fffffff :
            SIXEL_PIXELFORMAT_RGBFLOAT32);
        sixel_dither_set_pipeline_accumulation_buffer_hint(dither,
            history, sizeof(history), valid, sizeof(valid),
            1, 1, 0, 0, 1, 1, 2, 1, 0, SIXEL_6DELTA_ERROR_SKIP);
        status = sixel_encode((unsigned char *)pixels, 1, 1, 1, dither,
            output);
        if (dither->pipeline_6delta_hls ||
                (pass == 0 ? SIXEL_SUCCEEDED(status) : SIXEL_FAILED(status))) {
            goto end;
        }
    }
    ok = 1;
end:
    sixel_output_unref(output);
    sixel_dither_unref(dither);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}

int
test_delta_hls_fields(void)
{
    static double const sources[8][3] = {
        {.81, .31, .17}, {.31, .81, .17}, {.17, .81, .31},
        {.17, .31, .81}, {.31, .17, .81}, {.81, .17, .31},
        {.4, .4, .4}, {.123, .456, .789}
    };
    static int const fields[8][3] = {
        {133, 49, 65}, {227, 49, 65}, {253, 49, 65},
        {347, 49, 65}, {13, 49, 65}, {107, 49, 65},
        {0, 40, 0}, {330, 46, 73}
    };
    static unsigned char const decoded[8][3] = {
        {81, 31, 17}, {31, 81, 17}, {17, 81, 31},
        {17, 31, 81}, {31, 17, 81}, {81, 17, 31},
        {40, 40, 40}, {12, 46, 80}
    };
    int result[3];
    unsigned char rgb[3];
    int i;

    for (i = 0; i < 8; ++i) {
        sixel_color_rgb_to_hls(result, sources[i]);
        sixel_color_hls_to_rgb101(rgb, fields[i]);
        if (memcmp(result, fields[i], sizeof(result)) != 0 ||
                memcmp(rgb, decoded[i], sizeof(rgb)) != 0) {
            return EXIT_FAILURE;
        }
    }
    return EXIT_SUCCESS;
}
