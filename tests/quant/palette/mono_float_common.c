/* SPDX-License-Identifier: MIT */
/* Native monochrome coordinates must preserve foreground-mask semantics. */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sixel.h>
#include "src/encoder.h"
#include "src/dither.h"
#include "src/palette.h"
#include "tests/data/color-numeric/builtin-native.h"

static char const *const mn_spaces[] = {
    "gamma", "linear", "oklab", "cielab", "din99d"
};
static int const mn_formats[] = {
    SIXEL_PIXELFORMAT_RGBFLOAT32, SIXEL_PIXELFORMAT_LINEARRGBFLOAT32,
    SIXEL_PIXELFORMAT_OKLABFLOAT32, SIXEL_PIXELFORMAT_CIELABFLOAT32,
    SIXEL_PIXELFORMAT_DIN99DFLOAT32
};
static char const *const mn_outputs[] = {
    "gamma", "display-p3", "linear", "smpte-c"
};

typedef struct mn_capture {
    char data[4096];
    size_t size;
} mn_capture_t;

static int
mn_write(char *data, int size, void *context)
{
    mn_capture_t *capture;

    capture = context;
    if (size < 0 || (size_t)size >= sizeof(capture->data) - capture->size) {
        return -1;
    }
    memcpy(capture->data + capture->size, data, (size_t)size);
    capture->size += (size_t)size;
    capture->data[capture->size] = '\0';
    return size;
}

/* Interpret only the palette-free two-by-one output used by these probes.
 * A painted bit means foreground, independently of any decoder defaults. */
static int
mn_mask(char const *stream, int clear, int const *expected)
{
    char const *p;
    int x;
    int y;
    int repeat;
    int code;
    int mask[2];

    x = 0;
    y = 0;
    mask[0] = 0;
    mask[1] = 0;
    if (strchr(stream, '#') != NULL ||
            strstr(stream, clear ? ";0q" : ";1q") == NULL) {
        return 0;
    }
    p = strchr(stream, 'q');
    if (p == NULL) {
        return 0;
    }
    ++p;
    while (*p != '\0' && *p != '\033') {
        repeat = 1;
        if (*p == '"') {
            ++p;
            while ((*p >= '0' && *p <= '9') || *p == ';') {
                ++p;
            }
            continue;
        }
        if (*p == '$') {
            x = 0;
            ++p;
            continue;
        }
        if (*p == '-') {
            y += 6;
            x = 0;
            ++p;
            continue;
        }
        if (*p == '!') {
            repeat = 0;
            ++p;
            while (*p >= '0' && *p <= '9') {
                repeat = repeat * 10 + *p++ - '0';
                if (repeat > 2) {
                    return 0;
                }
            }
        }
        code = *p++ - '?';
        if (code < 0 || code > 1 || repeat < 1 || y != 0 ||
                x + repeat > 2) {
            return 0;
        }
        while (repeat-- > 0) {
            mask[x++] |= code;
        }
    }
    return mask[0] == expected[0] && mask[1] == expected[1];
}

static int
mn_values(sixel_dither_t *dither, int space, int invert)
{
    sixel_palette_float32_entries_view_t view;
    double expected;
    int entry;
    int channel;

    if (!dither->use_rgb101_palette || dither->ncolors != 2 ||
            dither->keycolor != 0 || dither->keycolor_reserved != 0 ||
            SIXEL_FAILED(dither->palette->vtbl->get_entries_float32(
                dither->palette, &view)) || view.entries == NULL ||
            view.entry_count != 2) {
        return 0;
    }
    for (entry = 0; entry < 2; ++entry) {
        for (channel = 0; channel < 3; ++channel) {
            expected = builtin_native[3].entries[space]
                [entry ^ invert][channel];
            /* Independent Decimal gray1 endpoints, stored as binary32.
             * Allow conversion error, not opponent-byte reconstruction. */
            if (!isfinite(view.entries[entry * 3 + channel]) ||
                    fabs(view.entries[entry * 3 + channel] - expected)
                        > 2e-6 + fabs(expected) * 5e-6) {
                return 0;
            }
        }
    }
    return 1;
}

static int
test_mono_native_run(int space, int output_kind, int invert, int mode,
                      int animated)
{
    static struct { int flag; char const *value; } const options[] = {
        { '=', "greedy:B1" }, { '~', "none" }, { 'd', "none" },
        { '0', "none" }, { '_', "none" }, { '.', "auto" },
        { 'A', "keep" }, { 'e', NULL }
    };
    sixel_encoder_t *encoder;
    sixel_output_t *output;
    sixel_frame_t *frame;
    sixel_dither_t *dither;
    mn_capture_t capture;
    float source[6];
    float saved[6];
    unsigned char bytes[6];
    unsigned char exported[6];
    SIXELSTATUS status;
    double weight;
    int passes;
    int pass;
    int selected_invert;
    int selected_output;
    int byte_input;
    int byte_work;
    int expected[2];
    int i;
    int j;
    int ok;

    encoder = NULL;
    output = NULL;
    frame = NULL;
    status = SIXEL_OK;
    ok = 0;
    memset(&capture, 0, sizeof(capture));
    passes = mode == 6 || mode == 7 ? 3 :
        mode == 8 || mode == 11 || mode == 12 ? 2 : 1;
    if (SIXEL_FAILED(sixel_encoder_new(&encoder, NULL)) ||
            SIXEL_FAILED(sixel_output_new(&output, mn_write,
                &capture, NULL))) {
        goto end;
    }
    for (i = 0; i < (int)(sizeof(options) / sizeof(options[0])); ++i) {
        if (SIXEL_FAILED(sixel_encoder_setopt(encoder, options[i].flag,
                options[i].value))) {
            goto end;
        }
    }
    if (SIXEL_FAILED(sixel_encoder_setopt(encoder, 'W', mn_spaces[space])) ||
            (mode == 5 && SIXEL_FAILED(sixel_encoder_setopt(encoder,
                '.', "float32"))) ||
            (mode >= 1 && mode <= 3 &&
             SIXEL_FAILED(sixel_encoder_setopt(encoder, 'd', "fs"))) ||
            (mode == 9 && SIXEL_FAILED(sixel_encoder_setopt(encoder,
                'A', "clear"))) ||
            (mode == 10 && SIXEL_FAILED(sixel_encoder_setopt(encoder,
                '_', "nearest:rate=0"))) ||
            (mode == 13 && SIXEL_FAILED(sixel_encoder_setopt(encoder,
                'M', "pal:-"))) ||
            (mode == 14 && SIXEL_FAILED(sixel_encoder_setopt(encoder,
                '~', "auto")))) {
        goto end;
    }
    if (animated) {
        passes = mode == 8 ? 2 : 3;
        encoder->fstatic = 1;
        encoder->fignore_delay = 1;
    }
    for (pass = 0; pass < passes; ++pass) {
        selected_invert = mode == 8 ? pass : invert;
        selected_output = mode == 7 ? (pass == 1 ? 1 : 0) : output_kind;
        byte_input = mode == 4 || mode == 5 || mode == 7 ||
            (mode == 6 && pass == 1);
        byte_work = byte_input && mode != 5 && selected_output != 1;
        expected[0] = 0;
        expected[1] = 1;
        for (i = 0; i < 2; ++i) {
            weight = mode == 15 ? .25 : mode == 16 ? .75 : .25 + .5 * i;
            if (mode == 15 || mode == 16) {
                expected[i] = mode == 16;
            }
            for (j = 0; j < 3; ++j) {
                bytes[i * 3 + j] = i == 0 ? 64 : 192;
                source[i * 3 + j] = (float)(
                    (1 - weight) * builtin_native[3].entries[space][0][j]
                    + weight * builtin_native[3].entries[space][1][j]);
                exported[i * 3 + j] = (unsigned char)
                    ((i ^ selected_invert) * 255);
            }
        }
        if (mode >= 1 && mode <= 3) {
            /* Ordinary: .2 -> 0, then .45 + .2*7/16 > .5.
             * Positive: 1.125 -> 1, then .45 + .125*7/16 > .5.
             * Negative: -.125 -> 0, then .55 - .125*7/16 < .5.
             * Margins exceed rounding error. Omitting feedback changes
             * each second decision; clipping changes the extended cases. */
            for (j = 0; j < 3; ++j) {
                source[j] = mode == 1 ? .2f : mode == 2 ? 1.125f : -.125f;
                source[j + 3] = mode == 3 ? .55f : .45f;
            }
            expected[0] = mode == 2;
            expected[1] = mode != 3;
        }
        expected[0] ^= selected_invert;
        expected[1] ^= selected_invert;
        memcpy(saved, source, sizeof(saved));
        memset(&capture, 0, sizeof(capture));
        if ((selected_invert && (!animated || pass == 0 || mode == 8) &&
             SIXEL_FAILED(sixel_encoder_setopt(encoder, 'i', NULL))) ||
                SIXEL_FAILED(sixel_encoder_setopt(encoder, 'U',
                    mn_outputs[selected_output])) ||
                SIXEL_FAILED(sixel_frame_new(&frame, NULL)) ||
                SIXEL_FAILED(sixel_frame_init_borrowed(frame,
                    byte_input ? (void *)bytes : (void *)source, 2, 1,
                    byte_input ? SIXEL_PIXELFORMAT_RGB888
                        : mn_formats[space], NULL, -1))) {
            goto end;
        }
        if (animated || (mode == 12 && pass == 0)) {
            sixel_frame_set_multiframe(frame, 1);
            sixel_frame_set_frame_no(frame, pass % 2);
            sixel_frame_set_loop_count(frame, pass / 2);
        }
        /* Fixed entries bypass construction snap in both passes. Verify
         * their independent values and painted indices/mask below. */
        if (mode == 11 && SIXEL_FAILED(sixel_encoder_setopt(encoder, '_',
                pass == 0 ? "nearest:rate=.25" : "none"))) {
            goto end;
        }
        status = sixel_encoder_encode_frame(encoder, frame, output);
        dither = (sixel_dither_t *)encoder->diagnostic_dither;
        if (SIXEL_FAILED(status) || dither == NULL ||
                memcmp(saved, source, sizeof(saved)) != 0 ||
                dither->use_rgb101_palette != !byte_work ||
                (!byte_work && !mn_values(dither, space, selected_invert)) ||
                !mn_mask(capture.data, mode == 9, expected)) {
            goto end;
        }
        if (mode == 13 && (encoder->capture_ncolors != 2 ||
                encoder->capture_palette_size != 6 ||
                memcmp(encoder->capture_palette, exported, 6) != 0)) {
            goto end;
        }
        sixel_frame_unref(frame);
        frame = NULL;
    }
    ok = 1;
end:
    if (!ok) {
        fprintf(stderr, "mono W=%d U=%d invert=%d mode=%d status=%x: "
            "%s; %s\n", space, output_kind, invert, mode, status,
            sixel_helper_get_additional_message(), capture.data);
    }
    sixel_frame_unref(frame);
    sixel_output_unref(output);
    sixel_encoder_unref(encoder);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}

int
test_mono_native(int space, int output_kind, int invert, int mode)
{
    return test_mono_native_run(space, output_kind, invert, mode, 0);
}

int
test_mono_animation(int space, int output_kind, int invert, int mode)
{
    return test_mono_native_run(space, output_kind, invert, mode, 1);
}
