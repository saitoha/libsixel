/* SPDX-License-Identifier: MIT */
/* Independent wire endpoints, parsed-state atomicity and ICC ownership. */
#include <float.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sixel.h>
#include "src/pixelformat.h"
#include "src/color-option.h"
#include "src/compat_stub.h"
#include "tests/data/color-numeric/icc-output.h"
#include "src/options-registry.h"
#include "src/encoder.h"
#include "src/encode-result.h"
#include "src/gamut.h"
#include "src/decoder.h"
#include "src/dither.h"
#include "src/palette.h"
#include "src/colorspace.h"
#include "src/palette-snap-grid.h"
#include "tests/data/color-numeric/vectors.h"
#include "tests/writer/png/color_test_common.h"

int test_dither_parallel_available(void);

static unsigned char icco_bytes[8192];
static size_t icco_size;

static int
icco_write(char *bytes, int size, void *context)
{
    (void)context;
    if (size < 0 || (size_t)size > sizeof(icco_bytes) - icco_size) {
        return -1;
    }
    memcpy(icco_bytes + icco_size, bytes, (size_t)size);
    icco_size += (size_t)size;
    return size;
}

static int
icco_encode(char const *argument, int mode)
{
    sixel_encoder_t *encoder;
    sixel_output_t *output;
    sixel_frame_t *frame;
    unsigned char *decoded;
    float pixels[9];
    float saved[9];
    int width;
    int height;
    int c;
    int pixel;
    int expected;
    int ok;
    SIXELSTATUS status;
    char replacement[4096];
    char const *slash;
    double converted[3];
    double input[3];
    sixel_palette_t *retained;

    retained = NULL;
    encoder = NULL;
    output = NULL;
    frame = NULL;
    decoded = NULL;
    ok = 0;
    icco_size = 0;
    /* Decimal oracle stores P3 primary XYZ and canonical linear sRGB.
     * Transform errors are far below the final 0.005 wire half bin. */
    for (pixel = 0; pixel < 3; ++pixel) {
        for (c = 0; c < 3; ++c) {
            pixels[pixel * 3 + c] = (float)color_vectors[pixel][9 + c];
        }
    }
    memcpy(saved, pixels, sizeof(saved));
    if (SIXEL_FAILED(sixel_encoder_new(&encoder, NULL)) ||
            SIXEL_FAILED(sixel_output_new(&output, icco_write, NULL, NULL)) ||
            SIXEL_FAILED(sixel_frame_new(&frame, NULL)) ||
            SIXEL_FAILED(sixel_encoder_setopt(encoder, 'Q', "kmeans")) ||
            SIXEL_FAILED(sixel_encoder_setopt(encoder, 'W', "linear")) ||
            SIXEL_FAILED(sixel_encoder_setopt(encoder, 'X', "linear")) ||
            SIXEL_FAILED(sixel_encoder_setopt(encoder, 'd', "none")) ||
            SIXEL_FAILED(sixel_encoder_setopt(encoder, 'a', "off")) ||
            SIXEL_FAILED(sixel_encoder_setopt(encoder, 'F', "none")) ||
            SIXEL_FAILED(sixel_encoder_setopt(encoder, '_', "none")) ||
            SIXEL_FAILED(sixel_encoder_setopt(encoder, 'U', argument))) {
        goto end;
    }
    if (mode == 2 || mode == 3 || mode == 6) {
        if (SIXEL_FAILED(sixel_encoder_setopt(encoder, '_',
                mode == 3 ? "reversible" : "nearest"))) {
            goto end;
        }
    }
    if (mode == 4 && SIXEL_FAILED(sixel_encoder_setopt(encoder, 'I', NULL))) {
        goto end;
    }
    if (mode == 5) {
        if (sixel_encoder_setopt(encoder, 'U', "icc:path=/nonexistent.icc")
                != SIXEL_BAD_ARGUMENT || encoder->output_icc == NULL) {
            goto end;
        }
    }
    if (SIXEL_FAILED(sixel_frame_init_borrowed(frame, pixels, 3, 1,
            SIXEL_PIXELFORMAT_LINEARRGBFLOAT32, NULL, -1))) {
        goto end;
    }
    status = sixel_encoder_encode_frame(encoder, frame, output);
    if (mode == 6) {
        ok = status == SIXEL_BAD_ARGUMENT && icco_size == 0;
        goto end;
    }
    if (SIXEL_FAILED(status) || memcmp(pixels, saved, sizeof(saved)) != 0 ||
            SIXEL_FAILED(sixel_decode_direct(icco_bytes, (int)icco_size,
                &decoded, &width, &height, NULL)) ||
            width != 3 || height != 1) {
        goto end;
    }
    for (pixel = 0; pixel < 3; ++pixel) {
        for (c = 0; c < 3; ++c) {
            /* The constant B2A1 fixture deliberately is not an inverse.
             * Its stored output table is exactly (13107,26214,39321)/65535. */
            expected = mode == 1 ? (c + 1) * 51 : pixel == c ? 255 : 0;
            if (decoded[pixel * 4 + c] != expected) {
                fprintf(stderr, "wire[%d,%d]=%u expected %d\n", pixel, c,
                    decoded[pixel * 4 + c], expected);
                goto end;
            }
        }
    }
    if (mode == 7) {
        if (encoder->diagnostic_dither == NULL) {
            goto end;
        }
        retained = ((sixel_dither_t *)encoder->diagnostic_dither)->palette;
        retained->vtbl->ref(retained);
        slash = strrchr(argument, '/');
        if (slash == NULL || (size_t)(slash - argument) > 4000u) {
            goto end;
        }
        snprintf(replacement, sizeof(replacement),
            "%.*s/relative-paired-lut.icc",
            (int)(slash - argument), argument);
        if (SIXEL_FAILED(sixel_encoder_set_accumulation_buffer(encoder,
                decoded, 3, 1, SIXEL_PIXELFORMAT_RGBA8888)) ||
                !encoder->accumulation_valid ||
                SIXEL_FAILED(sixel_encoder_setopt(encoder, 'U', replacement)) ||
                encoder->accumulation_valid) {
            goto end;
        }
        /* The retained palette must own its original transform even after
         * the encoder releases its source ICC and selects a different one. */
        for (c = 0; c < 3; ++c) {
            input[c] = color_vectors[0][9 + c];
        }
        if (!sixel_palette_to_output(retained, converted, input,
                SIXEL_COLORSPACE_LINEAR)) {
            goto end;
        }
        for (c = 0; c < 3; ++c) {
            if (fabs(converted[c] - (c == 0 ? 1.0 : 0.0)) > .0005) {
                goto end;
            }
        }
        icco_size = 0;
        free(decoded);
        decoded = NULL;
        if (SIXEL_FAILED(sixel_encoder_encode_frame(encoder, frame, output)) ||
                SIXEL_FAILED(sixel_decode_direct(icco_bytes, (int)icco_size,
                    &decoded, &width, &height, NULL))) {
            goto end;
        }
        for (pixel = 0; pixel < 3; ++pixel) {
            for (c = 0; c < 3; ++c) {
                if (decoded[pixel * 4 + c] != (c + 1) * 51) {
                    goto end;
                }
            }
        }
    }
    ok = 1;
end:
    if (retained != NULL) {
        retained->vtbl->unref(retained);
    }
    if (!ok) {
        fprintf(stderr, "%s\n", sixel_helper_get_additional_message());
    }
    free(decoded);
    sixel_frame_unref(frame);
    sixel_output_unref(output);
    sixel_encoder_unref(encoder);
    return ok;
}

static int
icco_parse(char const *argument, int decoder, int reject)
{
    sixel_color_def_t *definition;
    sixel_color_def_t *saved;
    int base;
    SIXELSTATUS status;
    int ok;

    definition = sixel_cdef_name("srgb");
    saved = definition;
    base = 9876;
    status = sixel_color_option_parse(argument, decoder, &base,
        &definition, NULL, NULL);
    if (reject) {
        ok = status == SIXEL_BAD_ARGUMENT && base == 9876 &&
            definition == saved;
    } else {
        ok = SIXEL_SUCCEEDED(status) && definition != NULL &&
            base == (decoder ? SIXEL_NAMED_COLOR_COUNT
                             : -SIXEL_NAMED_COLOR_COUNT);
    }
    if (definition != saved) {
        sixel_cdef_free(definition);
    }
    sixel_cdef_free(saved);
    return ok;
}

static int
icco_decoder(char const *argument)
{
    sixel_decoder_t *decoder;
    sixel_color_def_t *saved;
    double xyz[3];
    double rgb[3] = {1.0, 0.0, 0.0};
    int c;
    int ok;

    decoder = NULL;
    ok = 0;
    if (SIXEL_FAILED(sixel_decoder_new(&decoder, NULL)) ||
            SIXEL_FAILED(sixel_decoder_setopt(decoder, 'T', argument))) {
        goto end;
    }
    saved = decoder->input_icc;
    if (saved == NULL || sixel_decoder_setopt(decoder, 'T', "icc:path=-")
            != SIXEL_BAD_ARGUMENT || saved != decoder->input_icc ||
            !sixel_cdef_to_xyz(saved, rgb, xyz, 1)) {
        goto end;
    }
    /* Stored ICC matrix quantization: one half s15Fixed16 unit per field. */
    for (c = 0; c < 3; ++c) {
        if (fabs(xyz[c] - color_vectors[0][6 + c]) > 1.0 / 65536.0) {
            goto end;
        }
    }
    ok = SIXEL_SUCCEEDED(sixel_decoder_setopt(decoder, 'T', "srgb")) &&
        decoder->input_icc == NULL && decoder->input_colorspace == 0;
end:
    sixel_decoder_unref(decoder);
    return ok;
}

static int
icco_grid(char const *argument, int strategy)
{
    static unsigned int const codes[] = {0, 10 * 10201 + 50 * 101 + 90,
        100 * 10201 + 100 * 101 + 100};
    sixel_color_def_t *definition;
    sixel_snap_grid_t *grid;
    sixel_allocator_t *allocator;
    double actual[3];
    int base;
    int c;
    int i;
    int ok;

    definition = NULL;
    grid = NULL;
    allocator = NULL;
    ok = 0;
    if (SIXEL_FAILED(sixel_color_option_parse(argument, 0, &base,
            &definition, NULL, NULL)) ||
        SIXEL_FAILED(sixel_allocator_new(&allocator, NULL, NULL, NULL, NULL)) ||
        SIXEL_FAILED(sixel_snap_grid_new(&grid, strategy,
            SIXEL_COLORSPACE_LINEAR, definition, allocator))) {
        goto end;
    }
    sixel_cdef_free(definition);
    definition = NULL;
    for (i = 0; i < 3; ++i) {
        if (SIXEL_FAILED(sixel_snap_grid_target(grid, codes[i], actual))) {
            goto end;
        }
        for (c = 0; c < 3; ++c) {
            /* Stored ICC is exact; remaining error is double matrix math. */
            if (!isfinite(actual[c]) ||
                    fabs(actual[c] - icco_black_expected[i][c]) > 2e-9) {
                fprintf(stderr, "grid[%d,%d]=%.17g expected %.17g\n", i, c,
                    actual[c], icco_black_expected[i][c]);
                goto end;
            }
        }
    }
    ok = 1;
end:
    sixel_cdef_free(definition);
    sixel_snap_grid_destroy(grid);
    sixel_allocator_unref(allocator);
    return ok;
}

static int
icco_feedback_pixels(int hls, int first, int second)
{
    /* Independently round the device RGB tuple or its DEC HLS fields.
     * HLS (136,30,90)/(136,33,90) decode to RGB101 (57,17,3)/(63,19,3).
     * RGB device codes are (58,17,3)/(64,19,3). Convert at 255/100. */
    static unsigned char const expected[2][2][4] = {
        {{148, 43, 8, 255}, {163, 48, 8, 255}},
        {{145, 43, 8, 255}, {161, 48, 8, 255}},
    };
    unsigned char *decoded;
    int width;
    int height;
    int ok;

    decoded = NULL;
    ok = SIXEL_SUCCEEDED(sixel_decode_direct(icco_bytes, (int)icco_size,
             &decoded, &width, &height, NULL)) && width == 2 && height == 1;
    if (ok) {
        ok = memcmp(decoded, expected[hls][first], 4U) == 0 &&
            memcmp(decoded + 4, expected[hls][second], 4U) == 0;
        if (!ok) {
            fprintf(stderr, "feedback: got (%u,%u,%u), (%u,%u,%u); "
                    "expected palette entries %d,%d (HLS=%d)\n",
                    decoded[0], decoded[1], decoded[2],
                    decoded[4], decoded[5], decoded[6], first, second, hls);
        }
    }
    free(decoded);
    return ok;
}

static int
icco_register_feedback(char const *data, int mode)
{
    sixel_encoder_t *encoder;
    sixel_output_t *output;
    sixel_frame_t *frame;
    sixel_playback_t *playback;
    sixel_dither_t *cache;
    SIXELSTATUS status;
    float samples[6];
    char argument[4096];
    int pass;
    int hls;
    int ok;

    encoder = NULL;
    output = NULL;
    frame = NULL;
    playback = NULL;
    cache = NULL;
    hls = mode == 2;
    ok = 0;
    if (strlen(data) > sizeof(argument) - 64 ||
            SIXEL_FAILED(sixel_encoder_new(&encoder, NULL)) ||
            SIXEL_FAILED(sixel_output_new(&output, icco_write, NULL, NULL)) ||
            SIXEL_FAILED(sixel_frame_new(&frame, NULL)) ||
            SIXEL_FAILED(sixel_encoder_setopt(encoder, 'X', "oklab")) ||
            SIXEL_FAILED(sixel_encoder_setopt(encoder, 'W', "linear")) ||
            SIXEL_FAILED(sixel_encoder_setopt(encoder, '0', "none")) ||
            SIXEL_FAILED(sixel_encoder_setopt(encoder, 'F', "none")) ||
            SIXEL_FAILED(sixel_encoder_setopt(encoder, 't',
                                              hls ? "hls" : "rgb")) ||
            SIXEL_FAILED(sixel_encoder_setopt(encoder, 'd',
                       mode == 0 ? "none" : "fs:scan=raster"))) {
        goto end;
    }
    snprintf(argument, sizeof(argument),
             "icc:path=%s/register-feedback-lut.icc:bpc=0", data);
    if (SIXEL_FAILED(sixel_encoder_setopt(encoder, 'U', argument))) {
        goto end;
    }
    snprintf(argument, sizeof(argument), "gpl:%s/icc-register-fixed.gpl", data);
    if (SIXEL_FAILED(sixel_encoder_setopt(encoder, 'm', argument)) ||
            (mode == 3 && SIXEL_FAILED(sixel_playback_new(
                &playback, encoder, output, NULL, NULL)))) {
        goto end;
    }
    /* B2A maps each nominal linear gray y to (y,0.30*y,0.05*y).
     * A2B maps the output red code R back to gray (1+R/100)/2.
     * RGB: midpoint .805, second sample .803 -> .807375 with FS.
     * HLS: midpoint .800, second sample .797 -> .801375 with FS
     * using HLS-derived RGB. Using the RGB codes gives .7991875
     * against .805 and paints the wrong second color. */
    samples[0] = samples[1] = samples[2] = hls ? 0.795f : 0.800f;
    samples[3] = samples[4] = samples[5] = hls ? 0.797f : 0.803f;
    if (SIXEL_FAILED(sixel_frame_init_borrowed(frame, samples, 2, 1,
            SIXEL_PIXELFORMAT_LINEARRGBFLOAT32, NULL, -1))) {
        goto end;
    }
    /* The fixed-palette application cache lives within one animation.
     * Playback submissions marked as stills each start a new image. */
    sixel_frame_set_multiframe(frame, playback != NULL);
    for (pass = 0; pass < (mode == 3 ? 2 : 1); ++pass) {
        icco_size = 0;
        if (playback == NULL) {
            status = sixel_encoder_encode_frame(encoder, frame, output);
        } else {
            sixel_frame_set_frame_no(frame, pass);
            status = sixel_playback_submit(playback, frame,
                                           SIXEL_PLAYBACK_WAIT, -1, NULL);
            if (SIXEL_SUCCEEDED(status)) {
                status = sixel_playback_drain(playback, -1);
            }
        }
        if (SIXEL_FAILED(status) ||
                !icco_feedback_pixels(hls, pass, mode != 0 && pass == 0)) {
            goto end;
        }
        if (playback != NULL && pass == 0) {
            cache = encoder->dither_cache;
            if (cache == NULL) {
                fprintf(stderr, "feedback: fixed palette was not cached\n");
                goto end;
            }
            sixel_dither_ref(cache);
            /* First pixel is now high. Its negative residual shifts .806
             * to .8029375, below the RGB midpoint on the reused palette. */
            samples[0] = samples[1] = samples[2] = 0.813f;
            samples[3] = samples[4] = samples[5] = 0.806f;
        }
    }
    if (playback != NULL && encoder->dither_cache != cache) {
        fprintf(stderr, "feedback: fixed working palette was not reused\n");
        goto end;
    }
    ok = 1;
end:
    if (!ok) {
        fprintf(stderr, "register feedback mode %d: %s\n", mode,
                sixel_helper_get_additional_message());
    }
    sixel_playback_unref(playback);
    sixel_dither_unref(cache);
    sixel_frame_unref(frame);
    sixel_output_unref(output);
    sixel_encoder_unref(encoder);
    return ok;
}

static int
icco_delta_threshold(char const *path, int outside)
{
    float entries[6] = {.59f, .59f, .59f, 0, 0, 0};
    float query[3] = {.588f, .588f, .588f};
    float retained[3];
    unsigned char history[3] = {148, 43, 8};
    unsigned char bytes[6] = {0};
    unsigned char valid[1] = {1};
    unsigned char const *wire;
    unsigned int count;
    int ok;
    int channel;
    unsigned int threshold;
    sixel_dither_t *dither;
    sixel_color_def_t *profile;
    sixel_palette_float32_entries_request_t request;

    dither = NULL;
    profile = NULL;
    ok = 0;
    threshold = outside ? 0U : 1U;
    if (outside) {
        memcpy(query, entries, sizeof(query));
        history[0] = 151;
        history[1] = 45;
    }
    request.entries = entries;
    request.colors = 2;
    request.depth = 3 * sizeof(float);
    if (sixel_color_read_icc(path, &profile) != SIXEL_OK ||
            sixel_dither_new(&dither, 2, NULL) != SIXEL_OK) {
        goto end;
    }
    sixel_dither_set_palette(dither, bytes);
    if (dither->palette->vtbl->init_entries_float32(dither->palette,
                &request) != SIXEL_OK ||
            sixel_palette_prepare_output_definition(dither->palette,
                SIXEL_COLORSPACE_LINEAR, SIXEL_NAMED_COLOR_COUNT,
                SIXEL_COLORSPACE_GAMMA, profile) != SIXEL_OK ||
            sixel_palette_get_rgb101(dither->palette, &wire, &count)
                != SIXEL_OK || count != 2 ||
            wire[0] != 59 || wire[1] != 18 || wire[2] != 3) {
        goto end;
    }
    /* B2D maps .588 to (.588,.1764,.0294). The green-channel
     * difference from the retained byte 43 lies between 1/255 and 2/255.
     * With the .59 query, (151,45,8) is closer than (150,46,8)
     * in output RGB. D2B maps red back to .5 + R/2. The retained
     * gray is then farther away, so T0 must repaint and T1 keeps. */
    dither->use_rgb101_palette = 1;
    sixel_dither_set_pipeline_accumulation_buffer_hint(dither,
        history, sizeof(history), valid, sizeof(valid),
        1, 1, 0, 0, 1, 1, 2, 1, threshold, SIXEL_6DELTA_ERROR_DIFFUSE);
    if (sixel_dither_6delta_keep_float(dither, 0, 0, 0,
            SIXEL_PIXELFORMAT_LINEARRGBFLOAT32, query, entries, 0, 0,
            retained)) {
        goto end;
    }
    sixel_dither_set_pipeline_accumulation_buffer_hint(dither,
        history, sizeof(history), valid, sizeof(valid),
        1, 1, 0, 0, 1, 1, 2, 1, threshold + 1U, SIXEL_6DELTA_ERROR_DIFFUSE);
    if (!sixel_dither_6delta_keep_float(dither, 0, 0, 0,
            SIXEL_PIXELFORMAT_LINEARRGBFLOAT32, query, entries, 0, 0,
            retained)) {
        goto end;
    }
    for (channel = 0; channel < 3; ++channel) {
        if (!isfinite(retained[channel]) ||
                fabs(retained[channel] - (0.5 + history[0] / 510.0)) > 1e-5) {
            goto end;
        }
    }
    ok = 1;
end:
    sixel_cdef_free(profile);
    sixel_dither_unref(dither);
    return ok;
}

static int
icco_delta_feedback(char const *data, int mode)
{
    static unsigned char const expected_rgb[2][4] = {
        {148, 43, 8, 255}, {163, 48, 8, 255}
    };
    static unsigned char const expected_hls[2][4] = {
        {145, 43, 8, 255}, {161, 48, 8, 255}
    };
    static char const *const policies[2] = {
        "delta:T16:error=diffuse", "delta:T16:error=skip"
    };
    unsigned char history_row[6] = {204, 61, 10, 0, 0, 0};
    float sample_row[6] = {.80f, .80f, .80f, .82f, .82f, .82f};
    unsigned char history[12 * sizeof(history_row)];
    float samples[12 * 6];
    char argument[4096];
    sixel_encoder_t *encoder;
    sixel_output_t *output;
    sixel_frame_t *frame;
    sixel_dither_t const *dither;
    unsigned char *rgb;
    unsigned char *valid;
    unsigned char *decoded;
    unsigned char const *row;
    size_t offset;
    int width;
    int height;
    int rows;
    int pass;
    int y;
    int bands;
    int pipeline;
    int outside;
    int ok;

    encoder = NULL;
    output = NULL;
    frame = NULL;
    decoded = NULL;
    ok = 0;
    outside = mode == 1;
    bands = mode >= 2;
    pipeline = mode == 3;
    rows = bands ? 12 : 1;
    if (outside) {
        history_row[0] = 146;
        history_row[1] = 43;
        history_row[2] = 8;
        sample_row[0] = sample_row[1] = sample_row[2] = .55f;
    }
    for (y = 0; y < rows; ++y) {
        memcpy(history + y * sizeof(history_row), history_row,
               sizeof(history_row));
        memcpy(samples + y * 6, sample_row, sizeof(sample_row));
    }
    if (strlen(data) > sizeof(argument) - 64 ||
            sixel_encoder_new(&encoder, NULL) != SIXEL_OK ||
            sixel_output_new(&output, icco_write, NULL, NULL) != SIXEL_OK ||
            sixel_frame_new(&frame, NULL) != SIXEL_OK ||
            sixel_encoder_setopt(encoder, 'W', "linear") != SIXEL_OK ||
            sixel_encoder_setopt(encoder, 'G', "off") != SIXEL_OK ||
            sixel_encoder_setopt(encoder, '~', "none") != SIXEL_OK ||
            sixel_encoder_setopt(encoder, '_', "none") != SIXEL_OK ||
            sixel_encoder_setopt(encoder, 'F', "none") != SIXEL_OK ||
            sixel_encoder_setopt(encoder, 't', bands ? "hls" : "rgb")
                != SIXEL_OK ||
            sixel_encoder_setopt(encoder, 'd', bands
                ? "fs:scan=raster:band_width=6:band_overwrap=0"
                : "fs:scan=raster") != SIXEL_OK ||
            (bands && sixel_encoder_setopt(encoder, '=', pipeline
                ? "greedy:B4:dither=2:encode=2" : "greedy:B2") != SIXEL_OK)) {
        goto end;
    }
    snprintf(argument, sizeof(argument),
        "icc:P%s/register-feedback-lut.icc", data);
    if (sixel_encoder_setopt(encoder, 'U', argument) != SIXEL_OK) {
        goto end;
    }
    snprintf(argument, sizeof(argument), "gpl:%s/icc-register-fixed.gpl", data);
    if (sixel_encoder_setopt(encoder, 'm', argument) != SIXEL_OK) {
        goto end;
    }
    /* On each band start, .82 plus the retained residual
     * 7/16*(.80-.90) falls below both RGB and HLS midpoints.
     * At T0 the .55 query is closer to retained red 146 than
     * RGB candidate 148, but closer to HLS candidate 145. */
    for (pass = 0; pass < 2; ++pass) {
        icco_size = 0;
        if ((outside && sixel_encoder_setopt(encoder, 't',
                    pass == 0 ? "rgb" : "hls") != SIXEL_OK) ||
                sixel_encoder_setopt(encoder, 'Z', outside ?
                    "delta:T0:error=skip" : policies[pass]) != SIXEL_OK ||
                sixel_frame_init_borrowed(frame, samples, 2, rows,
                    SIXEL_PIXELFORMAT_LINEARRGBFLOAT32, NULL, -1)
                    != SIXEL_OK ||
                sixel_encoder_set_accumulation_buffer(encoder, history, 2,
                    rows, SIXEL_PIXELFORMAT_RGB888) != SIXEL_OK ||
                sixel_retained_plane_legacy_map(encoder->retained_plane,
                    &rgb, &valid) != SIXEL_OK) {
            goto end;
        }
        for (y = 0; y < rows * 2; ++y) {
            valid[y] = (unsigned char)(y % 12 == 0);
        }
        sixel_retained_plane_legacy_unmap(encoder->retained_plane);
        if (sixel_encoder_encode_frame(encoder, frame, output) != SIXEL_OK) {
            goto end;
        }
        dither = encoder->diagnostic_dither;
        if (bands && (dither == NULL || dither->pipeline_last_mode !=
                    (pipeline ? SIXEL_DITHER_ENCODE_PIPELINE
                              : SIXEL_DITHER_ENCODE_STAGED) ||
                dither->pipeline_last_dither_threads != 2 ||
                dither->pipeline_last_band_height != 6 ||
                dither->pipeline_last_band_overlap != 0)) {
            fprintf(stderr, "ICC HLS delta did not use two 6-row bands\n");
            goto end;
        }
        for (offset = 0; offset + 1 < icco_size; ++offset) {
            if (icco_bytes[offset] == 0x1b && icco_bytes[offset + 1] == 'P') {
                break;
            }
        }
        if (offset + 1 >= icco_size ||
                sixel_decode_direct(icco_bytes + offset,
                    (int)(icco_size - offset), &decoded, &width, &height, NULL)
                    != SIXEL_OK || width != 2 || height != rows) {
            goto end;
        }
        for (y = 0; y < rows; y += 6) {
            row = decoded + y * 8;
            if (outside) {
                if ((pass == 0 && row[3] != 0) ||
                        (pass == 1 && memcmp(row, expected_hls[0], 4) != 0)) {
                    fprintf(stderr, "ICC delta T0 pass %d: %u,%u,%u,%u\n",
                        pass, row[0], row[1], row[2], row[3]);
                    goto end;
                }
            } else if (row[3] != 0 || memcmp(row + 4,
                    bands ? expected_hls[pass] : expected_rgb[pass], 4) != 0) {
                fprintf(stderr, "Unexpected delta pixel in row %d\n", y);
                goto end;
            }
        }
        free(decoded);
        decoded = NULL;
    }
    ok = 1;
end:
    free(decoded);
    sixel_frame_unref(frame);
    sixel_output_unref(output);
    sixel_encoder_unref(encoder);
    return ok;
}

static int icco_fail_cache_malloc;

static void *
icco_cache_malloc(size_t size)
{
    return icco_fail_cache_malloc ? NULL : malloc(size);
}

static int
icco_cache_retry(char const *path, int copy)
{
    static unsigned char const linear[6] = {20,20,20,100,20,20};
    static unsigned char const icc[6] = {20,40,60,20,40,60};
    unsigned char bytes[6] = {51,51,51,255,51,51};
    float entries[6] = {.2f,.2f,.2f,1000000.0f,.2f,.2f};
    double query[3] = {.25,.25,.25};
    double forward[3];
    double reverse[3];
    double saved_reverse[3];
    unsigned char saved_hls[6];
    unsigned char const *codes;
    unsigned int count;
    int channel;
    int ok;
    sixel_allocator_t *allocator;
    sixel_dither_t *source;
    sixel_dither_t *target;
    sixel_color_def_t *profile;
    sixel_palette_float32_entries_request_t request;

    source = target = NULL;
    allocator = NULL;
    profile = NULL;
    ok = 0;
    request.entries = entries;
    request.colors = 2;
    request.depth = 3 * sizeof(float);
    if (sixel_color_read_icc(path, &profile) != SIXEL_OK ||
            sixel_dither_new(&source, 2, NULL) != SIXEL_OK) {
        goto end;
    }
    sixel_dither_set_palette(source, bytes);
    if (source->palette->vtbl->init_entries_float32(source->palette,
                &request) != SIXEL_OK ||
            sixel_palette_prepare_output_definition(source->palette,
                SIXEL_COLORSPACE_LINEAR, SIXEL_NAMED_COLOR_COUNT,
                SIXEL_COLORSPACE_GAMMA, profile) != SIXEL_OK) {
        goto end;
    }

    if (!copy) {
        if (sixel_palette_prepare_hls(source->palette) != SIXEL_OK ||
                sixel_palette_get_hls_rgb101(source->palette, &codes, &count)
                    != SIXEL_OK || count != 2 ||
                !sixel_palette_from_output(source->palette, saved_reverse,
                    query, SIXEL_COLORSPACE_LINEAR)) {
            goto end;
        }
        memcpy(saved_hls, codes, sizeof(saved_hls));
        /* The first color converts. The second is finite in linear RGB,
         * but overflows the exponential in the DIN99d inverse. */
        if (sixel_palette_prepare_output_definition(source->palette,
                    SIXEL_COLORSPACE_DIN99D, SIXEL_NAMED_COLOR_COUNT,
                    SIXEL_COLORSPACE_GAMMA, profile) != SIXEL_BAD_INPUT ||
                sixel_palette_get_rgb101(source->palette, &codes, &count)
                    != SIXEL_OK || count != 2 ||
                memcmp(codes, icc, sizeof(icc)) ||
                sixel_palette_get_hls_rgb101(source->palette, &codes, &count)
                    != SIXEL_OK || count != 2 ||
                memcmp(codes, saved_hls, sizeof(saved_hls)) ||
                !sixel_palette_from_output(source->palette, reverse, query,
                    SIXEL_COLORSPACE_LINEAR) ||
                memcmp(reverse, saved_reverse, sizeof(reverse)) ||
                sixel_palette_prepare_output_definition(source->palette,
                    SIXEL_COLORSPACE_LINEAR, SIXEL_NAMED_COLOR_COUNT,
                    SIXEL_COLORSPACE_GAMMA, profile) != SIXEL_OK) {
            goto end;
        }
    }
    sixel_cdef_free(profile);
    profile = NULL;

    if (copy) {
        if (sixel_palette_apply_icc_registers(source->palette,
                    SIXEL_COLORSPACE_LINEAR, SIXEL_COLORSPACE_LINEAR,
                    NULL, SIXEL_PALETTETYPE_HLS, 0, 0) != SIXEL_OK ||
                sixel_palette_get_hls_rgb101(source->palette, &codes, &count)
                    != SIXEL_OK || count != 2 ||
                !sixel_palette_from_output(source->palette, saved_reverse,
                    query, SIXEL_COLORSPACE_LINEAR) ||
                sixel_allocator_new(&allocator, icco_cache_malloc,
                    NULL, NULL, NULL) != SIXEL_OK ||
                sixel_dither_new(&target, 2, allocator) != SIXEL_OK) {
            goto end;
        }
        memcpy(saved_hls, codes, sizeof(saved_hls));
        sixel_dither_set_palette(target, bytes);
        if (target->palette->vtbl->init_entries_float32(target->palette,
                    &request) != SIXEL_OK ||
                sixel_palette_prepare_output(target->palette,
                    SIXEL_COLORSPACE_LINEAR, SIXEL_PALETTE_OUTPUT_LEGACY,
                    SIXEL_COLORSPACE_LINEAR) != SIXEL_OK) {
            goto end;
        }
        icco_fail_cache_malloc = 1;
        if (sixel_palette_copy_output_cache(target->palette, source->palette)
                    != SIXEL_BAD_ALLOCATION ||
                sixel_palette_get_rgb101(target->palette, &codes, &count)
                    != SIXEL_OK || count != 2 ||
                memcmp(codes, linear, sizeof(linear)) ||
                !sixel_palette_to_output(target->palette, forward, query,
                    SIXEL_COLORSPACE_LINEAR) ||
                memcmp(query, forward, sizeof(query))) {
            goto end;
        }
        icco_fail_cache_malloc = 0;
        if (sixel_palette_copy_output_cache(target->palette, source->palette)
                    != SIXEL_OK) {
            goto end;
        }
        sixel_dither_unref(source);
        source = target;
        target = NULL;
        if (sixel_palette_get_hls_rgb101(source->palette, &codes, &count)
                    != SIXEL_OK || count != 2 ||
                memcmp(codes, saved_hls, sizeof(saved_hls)) ||
                !sixel_palette_from_output(source->palette, reverse, query,
                    SIXEL_COLORSPACE_LINEAR) ||
                memcmp(reverse, saved_reverse, sizeof(reverse))) {
            goto end;
        }
    }
    if (sixel_palette_get_rgb101(source->palette, &codes, &count)
                != SIXEL_OK || count != 2 || memcmp(codes, icc, sizeof(icc)) ||
            !sixel_palette_to_output(source->palette, forward, query,
                SIXEL_COLORSPACE_LINEAR)) {
        goto end;
    }
    /* The fixture's B2A maps neutral input to fixed RGB (.2,.4,.6). */
    for (channel = 0; channel < 3; ++channel) {
        if (!isfinite(forward[channel]) ||
                fabs(forward[channel] - .2 * (channel + 1)) > 1e-6) {
            goto end;
        }
    }
    ok = 1;
end:
    icco_fail_cache_malloc = 0;
    sixel_cdef_free(profile);
    sixel_dither_unref(source);
    sixel_dither_unref(target);
    sixel_allocator_unref(allocator);
    return ok;
}

static int
icco_cache_unrounded(char const *path)
{
    static unsigned char const expected[6] = {59, 18, 3, 43, 13, 2};
    unsigned char bytes[6] = {150, 150, 150, 109, 109, 109};
    float entries[6] = {.5913f, .5913f, .5913f, .4271f, .4271f, .4271f};
    static double const factors[3] = {1.0, .30, .05};
    sixel_dither_t *source;
    sixel_dither_t *target;
    sixel_color_def_t *profile;
    sixel_palette_float32_entries_request_t request;
    unsigned char const *codes;
    unsigned char const *original;
    float const *wire_rgb;
    float const *original_float;
    unsigned int count;
    int channel;
    int i;
    int ok;

    source = target = NULL;
    profile = NULL;
    ok = 0;
    if (sixel_color_read_icc(path, &profile) != SIXEL_OK ||
            sixel_dither_new(&source, 2, NULL) != SIXEL_OK ||
            sixel_dither_new(&target, 2, NULL) != SIXEL_OK) {
        goto end;
    }
    sixel_dither_set_palette(source, bytes);
    sixel_dither_set_palette(target, bytes);
    request.entries = entries;
    request.colors = 2;
    request.depth = 3 * sizeof(float);
    if (source->palette->vtbl->init_entries_float32(source->palette,
                &request) != SIXEL_OK ||
            sixel_palette_prepare_output_definition(source->palette,
                SIXEL_COLORSPACE_LINEAR, SIXEL_NAMED_COLOR_COUNT,
                SIXEL_COLORSPACE_GAMMA, profile) != SIXEL_OK ||
            sixel_palette_apply_icc_registers(source->palette,
                SIXEL_COLORSPACE_LINEAR, SIXEL_COLORSPACE_LINEAR,
                NULL, SIXEL_PALETTETYPE_HLS, 0, 0) != SIXEL_OK ||
            sixel_palette_copy_output_cache(target->palette, source->palette)
                != SIXEL_OK) {
        goto end;
    }
    sixel_dither_unref(source);
    source = NULL;
    sixel_cdef_free(profile);
    profile = NULL;
    if (sixel_palette_get_rgb101(target->palette, &codes, &count)
                != SIXEL_OK || count != 2 ||
            memcmp(codes, expected, sizeof(expected)) != 0 ||
            sixel_palette_get_output_rgb(target->palette, &wire_rgb, &count)
                != SIXEL_OK || count != 2 ||
            !sixel_palette_get_icc_source(target->palette, &original,
                &original_float) ||
            memcmp(original, bytes, sizeof(bytes)) != 0 ||
            memcmp(original_float, entries, sizeof(entries)) != 0) {
        goto end;
    }
    /* Relative B2D is (Y, .30Y, .05Y) on this neutral input.
     * RGB101 cannot reconstruct any of these unrounded triplets. */
    for (i = 0; i < 6; ++i) {
        channel = i % 3;
        if (!isfinite(wire_rgb[i]) ||
                fabs(wire_rgb[i] - entries[i] * factors[channel]) > 1e-6) {
            goto end;
        }
    }
    ok = 1;
end:
    sixel_cdef_free(profile);
    sixel_dither_unref(source);
    sixel_dither_unref(target);
    return ok;
}

static int
icco_cache_rounded_inverse(char const *path)
{
    static unsigned char const saved[6] = {20, 20, 20, 85, 85, 85};
    unsigned char bytes[6] = {0};
    float entries[6] = {.2f, .2f, .2f, .848f, .848f, .848f};
    double continuous[3] = {.848, .848, 0};
    double rounded[3] = {217.0 / 255, 217.0 / 255, 0};
    double xyz[3];
    double query[3] = {.25, .25, .25};
    double out[3];
    sixel_dither_t *dither;
    sixel_color_def_t *profile;
    sixel_color_def_t *good;
    sixel_palette_float32_entries_request_t request;
    unsigned char const *codes;
    unsigned int count;
    int ok;

    dither = NULL;
    good = NULL;
    ok = 0;
    profile = sixel_cdef_icc(icco_rounding_profile,
                             sizeof(icco_rounding_profile));
    /* The second color crosses FLT_MAX in PCS X only after rounding. */
    if (profile == NULL || !sixel_cdef_output_ready(profile, 1, 1) ||
            !(2e38 * (continuous[0] + continuous[1]) < FLT_MAX) ||
            !(2e38 * (rounded[0] + rounded[1]) > FLT_MAX) ||
            !sixel_cdef_to_xyz(profile, continuous, xyz, 1) ||
            sixel_cdef_to_xyz(profile, rounded, xyz, 1) ||
            sixel_dither_new(&dither, 2, NULL) != SIXEL_OK) {
        goto end;
    }
    sixel_dither_set_palette(dither, bytes);
    request.entries = entries;
    request.colors = 2;
    request.depth = 3 * sizeof(float);
    if (dither->palette->vtbl->init_entries_float32(dither->palette,
                &request) != SIXEL_OK ||
            sixel_palette_prepare_output(dither->palette,
                SIXEL_COLORSPACE_LINEAR, SIXEL_PALETTE_OUTPUT_LEGACY,
                SIXEL_COLORSPACE_LINEAR) != SIXEL_OK ||
            sixel_palette_prepare_output_definition(dither->palette,
                SIXEL_COLORSPACE_LINEAR, SIXEL_NAMED_COLOR_COUNT,
                SIXEL_COLORSPACE_GAMMA, profile) != SIXEL_BAD_INPUT ||
            sixel_palette_get_rgb101(dither->palette, &codes, &count)
                != SIXEL_OK || count != 2 ||
            memcmp(codes, saved, sizeof(saved)) != 0 ||
            !sixel_palette_to_output(dither->palette, out, query,
                SIXEL_COLORSPACE_LINEAR) ||
            memcmp(out, query, sizeof(out)) != 0 ||
            sixel_color_read_icc(path, &good) != SIXEL_OK ||
            sixel_palette_prepare_output_definition(dither->palette,
                SIXEL_COLORSPACE_LINEAR, SIXEL_NAMED_COLOR_COUNT,
                SIXEL_COLORSPACE_GAMMA, good) != SIXEL_OK) {
        goto end;
    }
    ok = 1;
end:
    sixel_cdef_free(good);
    sixel_cdef_free(profile);
    sixel_dither_unref(dither);
    return ok;
}

static int
icco_direct_delta(char const *profile, char const *path)
{
    static unsigned char const expected[12] = {
        255, 0, 0, 255, 0, 255, 0, 255, 0, 0, 255, 255
    };
    float pixels[9];
    sixel_encoder_t *encoder;
    sixel_frame_t *frame;
    sixel_output_t *output;
    unsigned char *decoded;
    FILE *capture;
    size_t offset;
    int i;
    int width;
    int height;
    int ok;

    encoder = NULL;
    frame = NULL;
    output = NULL;
    decoded = NULL;
    capture = NULL;
    icco_size = 0;
    ok = 0;
    /* The shared Decimal oracle stores the target P3 primaries in
     * linear sRGB. These inputs must decode to the three P3 primaries. */
    for (i = 0; i < 9; ++i) {
        pixels[i] = (float)color_vectors[i / 3][9 + i % 3];
    }
    if (sixel_encoder_new(&encoder, NULL) != SIXEL_OK ||
            sixel_encoder_setopt(encoder, 'W', "linear") != SIXEL_OK ||
            sixel_encoder_setopt(encoder, 'X', "linear") != SIXEL_OK ||
            sixel_encoder_setopt(encoder, 'G', "off") != SIXEL_OK ||
            sixel_encoder_setopt(encoder, 'd', "none") != SIXEL_OK ||
            sixel_encoder_setopt(encoder, 'F', "none") != SIXEL_OK ||
            sixel_encoder_setopt(encoder, 'a', "off") != SIXEL_OK ||
            sixel_encoder_setopt(encoder, '_', "none") != SIXEL_OK ||
            sixel_encoder_setopt(encoder, '~', "none") != SIXEL_OK ||
            sixel_encoder_setopt(encoder, 'Z', "delta:P1") != SIXEL_OK ||
            sixel_encoder_setopt(encoder, 'U', profile) != SIXEL_OK) {
        goto end;
    }
    /* Neither direct call attaches a repair context. */
    if (path != NULL) {
        if (sixel_encoder_setopt(encoder, 'o', path) != SIXEL_OK ||
                sixel_encoder_encode_bytes(encoder, (unsigned char *)pixels,
                    3, 1, SIXEL_PIXELFORMAT_LINEARRGBFLOAT32, NULL, 0)
                    != SIXEL_OK) {
            goto end;
        }
        sixel_encoder_unref(encoder);
        encoder = NULL;
        capture = sixel_compat_fopen(path, "rb");
        if (capture == NULL) {
            goto end;
        }
        icco_size = fread(icco_bytes, 1, sizeof(icco_bytes), capture);
        if (ferror(capture) || icco_size == sizeof(icco_bytes)) {
            goto end;
        }
    } else if (sixel_output_new(&output, icco_write, NULL, NULL) != SIXEL_OK ||
            sixel_frame_new(&frame, NULL) != SIXEL_OK ||
            sixel_frame_init_borrowed(frame, pixels, 3, 1,
                SIXEL_PIXELFORMAT_LINEARRGBFLOAT32, NULL, -1) != SIXEL_OK ||
            sixel_encoder_encode_frame(encoder, frame, output) != SIXEL_OK) {
        goto end;
    }
    for (offset = 0; offset + 1 < icco_size; ++offset) {
        if (icco_bytes[offset] == 0x1b && icco_bytes[offset + 1] == 'P') {
            break;
        }
    }
    ok = offset + 1 < icco_size &&
        sixel_decode_direct(icco_bytes + offset, (int)(icco_size - offset),
            &decoded, &width, &height, NULL) == SIXEL_OK &&
        width == 3 && height == 1 &&
        memcmp(decoded, expected, sizeof(expected)) == 0;
end:
    if (!ok) {
        fprintf(stderr, "direct ICC delta: %s\n",
                sixel_helper_get_additional_message());
    }
    if (capture != NULL) {
        fclose(capture);
    }
    free(decoded);
    sixel_frame_unref(frame);
    sixel_output_unref(output);
    sixel_encoder_unref(encoder);
    return ok;
}

static int
icco_progressive_reject(char const *argument)
{
    sixel_progressive_config_t config;
    sixel_encoder_t *encoder;
    sixel_progressive_t *context;
    int ok;

    encoder = NULL;
    context = NULL;
    ok = sixel_progressive_config_init(&config, sizeof(config)) == SIXEL_OK &&
        sixel_encoder_new(&encoder, NULL) == SIXEL_OK &&
        sixel_encoder_setopt(encoder, 'Z', "delta") == SIXEL_OK &&
        sixel_encoder_setopt(encoder, 'U', argument) == SIXEL_OK &&
        sixel_progressive_new(&context, encoder, &config, NULL)
            == SIXEL_BAD_ARGUMENT && context == NULL &&
        sixel_encoder_setopt(encoder, 'U', "gamma") == SIXEL_OK &&
        sixel_progressive_new(&context, encoder, &config, NULL) == SIXEL_OK &&
        sixel_encoder_setopt(encoder, 'U', argument) == SIXEL_BAD_ARGUMENT &&
        encoder->output_icc == NULL;
    sixel_progressive_unref(context);
    sixel_encoder_unref(encoder);
    return ok;
}

static uintptr_t icco_watched_mapper;
static unsigned int icco_mapper_frees;

static void
icco_watch_free(void *object)
{
    if (object != NULL && (uintptr_t)object == icco_watched_mapper) {
        ++icco_mapper_frees;
    }
    free(object);
}

static int
icco_progressive_retry(char const *argument)
{
    static double const outside[3] = {1.4, -.2, .3};
    float samples[6] = {1.4f, -.2f, .3f, .2f, .2f, .2f};
    double expected[3];
    double mapped[3];
    sixel_progressive_config_t config;
    sixel_allocator_t *allocator;
    sixel_encoder_t *encoder;
    sixel_progressive_t *context;
    sixel_frame_t *frame;
    sixel_output_t *output;
    sixel_gamut_t *mapper;
    int ok;

    allocator = NULL;
    encoder = NULL;
    context = NULL;
    frame = NULL;
    output = NULL;
    ok = 0;
    icco_watched_mapper = 0;
    icco_mapper_frees = 0;
    if (sixel_allocator_new(&allocator, NULL, NULL, NULL, icco_watch_free)
                != SIXEL_OK ||
            sixel_encoder_new(&encoder, allocator) != SIXEL_OK ||
            sixel_encoder_setopt(encoder, 'W', "linear") != SIXEL_OK ||
            sixel_encoder_setopt(encoder, 'G', "off") != SIXEL_OK ||
            sixel_encoder_setopt(encoder, 'd', "none") != SIXEL_OK ||
            sixel_encoder_setopt(encoder, '~', "none") != SIXEL_OK ||
            sixel_encoder_setopt(encoder, '_', "none") != SIXEL_OK ||
            sixel_encoder_setopt(encoder, 'Z', "delta") != SIXEL_OK ||
            sixel_encoder_setopt(encoder, 'U', "gamma:gamut=local-minde")
                != SIXEL_OK ||
            sixel_progressive_config_init(&config, sizeof(config))
                != SIXEL_OK ||
            sixel_progressive_new(&context, encoder, &config, allocator)
                != SIXEL_OK) {
        goto end;
    }
    mapper = encoder->gamut_mapper;
    if (mapper == NULL || !sixel_gamut_apply(mapper, outside, expected,
            SIXEL_COLORSPACE_LINEAR) ||
            memcmp(outside, expected, sizeof(expected)) == 0) {
        goto end;
    }
    icco_watched_mapper = (uintptr_t)mapper;
    icco_size = 0;
    if (sixel_encoder_setopt(encoder, 'U', argument) != SIXEL_BAD_ARGUMENT ||
            icco_mapper_frees != 0 || encoder->gamut_mapper != mapper ||
            encoder->output_icc != NULL ||
            !sixel_gamut_apply(mapper, outside, mapped, SIXEL_COLORSPACE_LINEAR)
                || memcmp(expected, mapped, sizeof(mapped)) != 0 ||
            sixel_output_new(&output, icco_write, NULL, allocator)
                != SIXEL_OK ||
            sixel_frame_new(&frame, allocator) != SIXEL_OK ||
            sixel_frame_init_borrowed(frame, samples, 2, 1,
                SIXEL_PIXELFORMAT_LINEARRGBFLOAT32, NULL, -1) != SIXEL_OK ||
            sixel_encoder_encode_frame(encoder, frame, output) != SIXEL_OK ||
            icco_size == 0 || icco_mapper_frees != 0) {
        goto end;
    }
    ok = 1;
end:
    sixel_frame_unref(frame);
    sixel_output_unref(output);
    sixel_progressive_unref(context);
    sixel_encoder_unref(encoder);
    sixel_allocator_unref(allocator);
    return ok && icco_mapper_frees == 1;
}

/* Distinguish the exact legacy result (42) from a fixed target (0).
 * A decoder error or any third result is a real failure. */
static int
icco_output_target(int cluster, char const *data)
{
    static unsigned char const old_cluster[] = {0, 161, 161};
    static unsigned char const new_cluster[] = {64, 64, 204};
    static unsigned char const low[] = {148, 43, 8, 255};
    static unsigned char const high[] = {163, 48, 8, 255};
    float samples[9];
    sixel_encoder_t *encoder = NULL;
    sixel_output_t *output = NULL;
    sixel_frame_t *frame = NULL;
    sixel_dither_t const *applied;
    unsigned char *decoded = NULL;
    char arg[4096];
    int width, height, c, i;
    int known, desired;
    int result = 1;

    icco_size = 0;
    if (strlen(data) > sizeof(arg) - 64 ||
            SIXEL_FAILED(sixel_encoder_new(&encoder, NULL)) ||
            SIXEL_FAILED(sixel_output_new(&output, icco_write, NULL, NULL)) ||
            SIXEL_FAILED(sixel_frame_new(&frame, NULL)) ||
            sixel_encoder_setopt(encoder, 'X', "linear") ||
            sixel_encoder_setopt(encoder, 'W', "linear") ||
            sixel_encoder_setopt(encoder, 'F', "none") ||
            sixel_encoder_setopt(encoder, '_', "none") ||
            sixel_encoder_setopt(encoder, '0', "none") ||
            sixel_encoder_setopt(encoder, 't', "rgb") ||
            sixel_encoder_setopt(encoder, 'd',
                                 cluster ? "none" : "fs:scan=raster")) {
        goto end;
    }
    if (cluster) {
        /* In linear -X, (-.8, .1, .6) clusters into (-.8, .35).
         * Clipping to the sRGB gamut first yields (.05, .6).
         * Apply the sRGB transfer and round to whole device percentages. */
        if (sixel_encoder_setopt(encoder, 'a', "off") ||
                sixel_encoder_setopt(encoder, '4', "full-frame") ||
                sixel_encoder_setopt(encoder, '5', "none") ||
                sixel_encoder_setopt(encoder, 'U', "gamma") ||
                sixel_encoder_setopt(encoder, 'Q', "kmeans:seed=1") ||
                sixel_encoder_setopt(encoder, 'p', "2")) {
            goto end;
        }
        for (c = 0; c < 3; ++c) {
            samples[c] = -0.8f;
            samples[3 + c] = 0.1f;
            samples[6 + c] = 0.6f;
        }
    } else {
        /* B2A red = y; A2B gray = (1 + red)/2. Targets .800 and
         * .802 bracket the .805 decision after .010 * 7/16 carry.
         * Rounding .604 to .60 before A2B would miss the boundary. */
        snprintf(arg, sizeof(arg), "icc:P%s/register-feedback-lut.icc", data);
        if (sixel_encoder_setopt(encoder, 'U', arg)) {
            goto end;
        }
        snprintf(arg, sizeof(arg), "gpl:%s/icc-register-fixed.gpl", data);
        if (sixel_encoder_setopt(encoder, 'm', arg)) {
            goto end;
        }
        for (c = 0; c < 3; ++c) {
            samples[c] = 0.600f;
            samples[3 + c] = 0.604f;
        }
    }
    if (sixel_frame_init_borrowed(frame, samples, cluster ? 3 : 2, 1,
                SIXEL_PIXELFORMAT_LINEARRGBFLOAT32, NULL, -1) ||
            sixel_encoder_encode_frame(encoder, frame, output) ||
            sixel_decode_direct(icco_bytes, (int)icco_size, &decoded,
                &width, &height, NULL) || width != (cluster ? 3 : 2) ||
            height != 1) {
        goto end;
    }
    if (cluster) {
        known = desired = 1;
        for (i = 0; i < width; ++i) {
            for (c = 0; c < 3; ++c) {
                known &= decoded[i * 4 + c] == old_cluster[i];
                desired &= decoded[i * 4 + c] == new_cluster[i];
            }
            known &= decoded[i * 4 + 3] == 255;
            desired &= decoded[i * 4 + 3] == 255;
        }
    } else {
        /* Read the retained dither for the emitting frame. Requested
         * options alone would also pass if FS silently fell back to none. */
        applied = encoder->diagnostic_dither;
        if (applied == NULL || applied->method_for_diffuse != SIXEL_DIFFUSE_FS
                || applied->method_for_scan != SIXEL_SCAN_RASTER ||
                applied->apply_success_count == 0) {
            fprintf(stderr, "Raster FS was not applied\n");
            goto end;
        }
        known = !memcmp(decoded, low, 4) && !memcmp(decoded + 4, low, 4);
        desired = !memcmp(decoded, low, 4) && !memcmp(decoded + 4, high, 4);
    }
    result = desired ? 0 : known ? 42 : 1;
    if (result == 1) {
        fprintf(stderr, "Unexpected target pixels:");
        for (i = 0; i < width * 4; ++i) {
            fprintf(stderr, " %u", decoded[i]);
        }
        fprintf(stderr, "\n");
    }
end:
    if (result == 1) {
        fprintf(stderr, "target fixture: %s\n",
                sixel_helper_get_additional_message());
    }
    free(decoded);
    sixel_frame_unref(frame);
    sixel_output_unref(output);
    sixel_encoder_unref(encoder);
    return result;
}

/* Observe explicit FS on a generated two-color palette. A writer may omit
 * an unused register when both target pixels resolve to the same color. */
static int
icco_exact_target(char *trace)
{
    static unsigned char const low[] = {128, 128, 128, 255};
    static unsigned char const high[] = {130, 130, 130, 255};
    unsigned char *pixels = NULL;
    char *dcs;
    char *end;
    int width, height;
    int result = 1;

    dcs = strstr(trace, "\033P");
    end = dcs == NULL ? NULL : strstr(dcs, "\033\\");
    if (end != NULL && strstr(dcs, ";2;50;50;50") != NULL &&
            !sixel_decode_direct((unsigned char *)dcs, (int)(end - dcs + 2),
                &pixels, &width, &height, NULL) && width == 2 && height == 1 &&
            memcmp(pixels, low, 4) == 0) {
        if (strstr(trace, "LSXDTH1|rc=0|diffuse=fs|") != NULL &&
                memcmp(pixels + 4, low, 4) == 0) {
            result = 0;
        } else if (strstr(trace, "LSXDTH1|rc=0|diffuse=none|") != NULL &&
                strstr(dcs, ";2;51;51;51") != NULL &&
                memcmp(pixels + 4, high, 4) == 0) {
            result = 42;
        }
    }
    free(pixels);
    return result;
}

int
test_icc_output(int argc, char **argv)
{
    int mode;

    if (argc == 3 && strcmp(argv[1], "exact-target") == 0) {
        return icco_exact_target(argv[2]);
    }
    if (argc != 4) {
        return 1;
    }
    if (strcmp(argv[1], "output-target") == 0) {
        return icco_output_target(strcmp(argv[2], "cluster") == 0, argv[3]);
    }
    if (strcmp(argv[1], "direct-delta") == 0) {
        return !icco_direct_delta(argv[2],
            strcmp(argv[3], "-") == 0 ? NULL : argv[3]);
    }
    mode = atoi(argv[2]);
    if (strcmp(argv[1], "delta-threshold") == 0) {
        return !icco_delta_threshold(argv[3], mode);
    }
    if (strcmp(argv[1], "delta-feedback") == 0) {
        if (mode >= 2 && !test_dither_parallel_available()) {
            return 77;
        }
        return !icco_delta_feedback(argv[3], mode);
    }
    if (strcmp(argv[1], "cache-unrounded") == 0) {
        return !icco_cache_unrounded(argv[3]);
    }
    if (strcmp(argv[1], "cache-rounded-inverse") == 0) {
        return !icco_cache_rounded_inverse(argv[3]);
    }
    if (strcmp(argv[1], "cache-retry") == 0) {
        return !icco_cache_retry(argv[3], mode);
    }
    if (strcmp(argv[1], "progressive-retry") == 0) {
        return !icco_progressive_retry(argv[3]);
    }
    if (strcmp(argv[1], "progressive-reject") == 0) {
        return !icco_progressive_reject(argv[3]);
    }
    if (strcmp(argv[1], "register-feedback") == 0) {
        return !icco_register_feedback(argv[3], mode);
    }
    if (strcmp(argv[1], "encode") == 0) {
        return !icco_encode(argv[3], mode);
    }
    if (strcmp(argv[1], "parse") == 0) {
        return !icco_parse(argv[3], mode & 1, mode > 1);
    }
    if (strcmp(argv[1], "decoder") == 0) {
        return !icco_decoder(argv[3]);
    }
    if (strcmp(argv[1], "grid") == 0) {
        return !icco_grid(argv[3], mode);
    }
    if (strcmp(argv[1], "png") == 0) {
        return png_color_check(1, mode == 0 ? 0 : mode == 1 ? 1 : 2,
            mode < 2 ? 0 : mode);
    }
    if (strcmp(argv[1], "snapshot") == 0) {
        return mode ? png_snapshot_direct_check(1, 0)
                    : png_snapshot_check(1, 0);
    }
    return 1;
}
