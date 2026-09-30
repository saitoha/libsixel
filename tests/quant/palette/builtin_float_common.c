/* SPDX-License-Identifier: MIT */
/* Built-in tables retain their sRGB meaning in native palette application. */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sixel.h>
#include "src/encoder.h"
#include "src/dither.h"
#include "src/palette.h"
#include "tests/data/color-numeric/builtin-native.h"

static char const *const bi_spaces[] = {
    "gamma", "linear", "oklab", "cielab", "din99d"
};
static int const bi_formats[] = {
    SIXEL_PIXELFORMAT_RGBFLOAT32, SIXEL_PIXELFORMAT_LINEARRGBFLOAT32,
    SIXEL_PIXELFORMAT_OKLABFLOAT32, SIXEL_PIXELFORMAT_CIELABFLOAT32,
    SIXEL_PIXELFORMAT_DIN99DFLOAT32
};
static char const *const bi_outputs[] = {
    "gamma", "display-p3", "linear", "smpte-c"
};
static char const *const bi_tables[] = {
    "xterm16", "gray2", "vt340mono", "gray1"
};

typedef struct bi_capture {
    char data[8192];
    size_t size;
} bi_capture_t;

static int
bi_write(char *data, int size, void *context)
{
    bi_capture_t *capture;

    capture = context;
    if (size < 0 || (size_t)size >= sizeof(capture->data) - capture->size) {
        return -1;
    }
    memcpy(capture->data + capture->size, data, (size_t)size);
    capture->size += (size_t)size;
    capture->data[capture->size] = '\0';
    return size;
}

static int
bi_values(sixel_dither_t *dither, bi_capture_t *capture,
          int table, int space, int output)
{
    sixel_palette_float32_entries_view_t view;
    unsigned char const *wire;
    unsigned int count;
    unsigned int entry;
    unsigned int channel;
    double expected;
    char definition[64];

    if (!dither->use_rgb101_palette ||
            SIXEL_FAILED(dither->palette->vtbl->get_entries_float32(
                dither->palette, &view)) || view.entries == NULL ||
            view.entry_count != builtin_native[table].count ||
            SIXEL_FAILED(sixel_palette_get_rgb101(dither->palette,
                &wire, &count)) || count != view.entry_count) {
        fprintf(stderr, "native palette view missing, rgb101=%d\n",
            dither->use_rgb101_palette);
        return 0;
    }
    for (entry = 0; entry < count; ++entry) {
        for (channel = 0; channel < 3; ++channel) {
            expected = builtin_native[table].entries[space][entry][channel];
            /* Independent Decimal transforms start from stored binary32
             * sRGB8/255. This budget covers conversion/storage error, far
             * below compatibility-byte projection of Lab opponents. */
            if (!isfinite(view.entries[entry * 3 + channel]) ||
                    fabs(view.entries[entry * 3 + channel] - expected)
                        > 2e-6 + fabs(expected) * 5e-6) {
                fprintf(stderr, "entry %u/%u: %.9g expected %.9g\n",
                    entry, channel, (double)view.entries[entry * 3 + channel],
                    expected);
                return 0;
            }
        }
        if (memcmp(wire + entry * 3,
                builtin_native[table].wire[space][output][entry], 3) != 0) {
            fprintf(stderr, "wire %u: %u %u %u expected %u %u %u\n", entry,
                wire[entry * 3], wire[entry * 3 + 1], wire[entry * 3 + 2],
                builtin_native[table].wire[space][output][entry][0],
                builtin_native[table].wire[space][output][entry][1],
                builtin_native[table].wire[space][output][entry][2]);
            return 0;
        }
        snprintf(definition, sizeof(definition), "#%u;2;%u;%u;%u", entry,
            (unsigned)wire[entry * 3], (unsigned)wire[entry * 3 + 1],
            (unsigned)wire[entry * 3 + 2]);
        if (strstr(capture->data, definition) == NULL) {
            return 0;
        }
    }
    return 1;
}

int
test_builtin_native(int space, int output_kind, int table, int mode)
{
    static struct { int flag; char const *value; } const options[] = {
        { '=', "1" }, { '~', "none" }, { 'd', "none" },
        { '0', "none" }, { '_', "none" }, { '.', "auto" }
    };
    sixel_encoder_t *encoder;
    sixel_output_t *output;
    sixel_frame_t *frame;
    sixel_dither_t *dither;
    bi_capture_t capture;
    float source[6];
    float saved[6];
    unsigned char bytes[6];
    unsigned char *decoded;
    unsigned char *palette;
    SIXELSTATUS status;
    int count;
    int width;
    int height;
    int passes;
    int pass;
    int selected_table;
    int selected_output;
    int byte_input;
    int byte_work;
    int expected[2];
    int entry;
    int i;
    int j;
    int ok;

    encoder = NULL;
    output = NULL;
    frame = NULL;
    decoded = NULL;
    palette = NULL;
    status = SIXEL_OK;
    ok = 0;
    memset(&capture, 0, sizeof(capture));
    passes = mode >= 4 && mode <= 6 ? 3 : mode == 8 ? 2 : 1;
    if (SIXEL_FAILED(sixel_encoder_new(&encoder, NULL)) ||
            SIXEL_FAILED(sixel_output_new(&output, bi_write,
                &capture, NULL))) {
        goto end;
    }
    for (i = 0; i < (int)(sizeof(options) / sizeof(options[0])); ++i) {
        if (SIXEL_FAILED(sixel_encoder_setopt(encoder, options[i].flag,
                options[i].value))) {
            goto end;
        }
    }
    if (SIXEL_FAILED(sixel_encoder_setopt(encoder, 'W', bi_spaces[space])) ||
            (mode == 2 && SIXEL_FAILED(sixel_encoder_setopt(encoder,
                '.', "float32"))) ||
            (mode == 3 && SIXEL_FAILED(sixel_encoder_setopt(encoder,
                '_', "nearest:rate=0"))) ||
            (mode >= 10 && SIXEL_FAILED(sixel_encoder_setopt(encoder,
                'd', "fs"))) ||
            (mode == 9 && SIXEL_FAILED(sixel_encoder_setopt(encoder,
                'M', "pal:-")))) {
        goto end;
    }
    for (pass = 0; pass < passes; ++pass) {
        selected_table = mode == 6 ? pass : table;
        selected_output = mode == 5 ? (pass == 1 ? 0 : 1) : output_kind;
        byte_input = mode == 1 || mode == 2 || mode == 5 ||
            (mode == 4 && pass == 1);
        byte_work = (byte_input && mode != 2 && selected_output != 1) ||
            (mode == 8 && pass == 0);
        expected[0] = selected_table == 0 ? 9 : 1;
        expected[1] = selected_table == 0 ? 8 : 0;
        for (i = 0; i < 2; ++i) {
            for (j = 0; j < 3; ++j) {
                bytes[i * 3 + j] =
                    builtin_native[selected_table].rgb[expected[i]][j];
                source[i * 3 + j] = (float)builtin_native[selected_table]
                    .entries[space][expected[i]][j];
            }
        }
        if (mode >= 10) {
            /* Gray2: .2 -> 1/3, then .22 - (2/15)*7/16 < 1/6.
             * Gray1 positive: 1.125 -> 1, then .45 + .125*7/16 > .5.
             * Gray1 negative: -.125 -> 0, then .55 - .125*7/16 < .5.
             * All choices have margins larger than float storage error;
             * omitting diffusion or clipping the source changes pixel 2. */
            for (j = 0; j < 3; ++j) {
                source[j] = mode == 10 ? .2f : mode == 11 ? 1.125f : -.125f;
                source[j + 3] = mode == 10 ? .22f : mode == 11 ? .45f : .55f;
            }
            expected[0] = mode == 12 ? 0 : 1;
            expected[1] = mode == 11 ? 1 : 0;
        }
        memcpy(saved, source, sizeof(saved));
        memset(&capture, 0, sizeof(capture));
        if (SIXEL_FAILED(sixel_encoder_setopt(encoder, 'b',
                bi_tables[selected_table])) ||
                SIXEL_FAILED(sixel_encoder_setopt(encoder, 'U',
                    bi_outputs[selected_output])) ||
                SIXEL_FAILED(sixel_frame_new(&frame, NULL)) ||
                SIXEL_FAILED(sixel_frame_init_borrowed(frame,
                    byte_input ? (void *)bytes : (void *)source, 2, 1,
                    byte_input ? SIXEL_PIXELFORMAT_RGB888
                        : bi_formats[space], NULL, -1))) {
            goto end;
        }
        if (mode == 8 && pass == 0) {
            sixel_frame_set_multiframe(frame, 1);
        }
        if (mode == 7) {
            if (SIXEL_FAILED(sixel_encoder_setopt(encoder, '_', "nearest")) ||
                    !SIXEL_FAILED(sixel_encoder_encode_frame(encoder,
                        frame, output)) || capture.size != 0 ||
                    SIXEL_FAILED(sixel_encoder_setopt(encoder,
                        '_', "none"))) {
                goto end;
            }
        }
        status = sixel_encoder_encode_frame(encoder, frame, output);
        dither = (sixel_dither_t *)encoder->diagnostic_dither;
        if (SIXEL_FAILED(status) || dither == NULL ||
                memcmp(saved, source, sizeof(saved)) != 0 ||
                dither->use_rgb101_palette != !byte_work ||
                (!byte_work && !bi_values(dither, &capture, selected_table,
                    space, selected_output))) {
            goto end;
        }
        if (mode == 9 && (encoder->capture_ncolors != 16 ||
                encoder->capture_palette_size != 48 ||
                memcmp(encoder->capture_palette,
                    builtin_native[0].rgb, 48) != 0)) {
            goto end;
        }
        if (SIXEL_FAILED(sixel_decode_raw((unsigned char *)capture.data,
                (int)capture.size, &decoded, &width, &height,
                &palette, &count, NULL)) || width != 2 || height != 1) {
            goto end;
        }
        if (!byte_work) {
            for (i = 0; i < 2; ++i) {
                entry = decoded[i];
                /* Duplicate VT340 grays may select either identical entry.
                 * Keep their table slots without prescribing tie order. */
                if (entry >= (int)builtin_native[selected_table].count ||
                        memcmp(builtin_native[selected_table].rgb[entry],
                            builtin_native[selected_table].rgb[expected[i]],
                            3) != 0) {
                    fprintf(stderr, "pixel %d: %d expected %d\n",
                        i, entry, expected[i]);
                    goto end;
                }
            }
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
        fprintf(stderr, "builtin W=%d U=%d table=%d mode=%d status=%x: "
            "%s; %s\n", space, output_kind, table, mode, status,
            sixel_helper_get_additional_message(), capture.data);
    }
    free(decoded);
    free(palette);
    sixel_frame_unref(frame);
    sixel_output_unref(output);
    sixel_encoder_unref(encoder);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
