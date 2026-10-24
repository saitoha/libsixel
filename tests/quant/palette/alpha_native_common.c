/* SPDX-License-Identifier: MIT */
/* Independent colors and painted indexes for separated alpha-zero masks. */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sixel.h>
#include "src/encoder.h"
#include "src/dither.h"
#include "src/palette.h"
#include "src/palette-kmeans.h"
#include "tests/data/color-numeric/output-native.h"

static char const *const an_spaces[] = {
    "gamma", "linear", "oklab", "cielab", "din99d"
};
static int const an_formats[] = {
    SIXEL_PIXELFORMAT_RGBFLOAT32, SIXEL_PIXELFORMAT_LINEARRGBFLOAT32,
    SIXEL_PIXELFORMAT_OKLABFLOAT32, SIXEL_PIXELFORMAT_CIELABFLOAT32,
    SIXEL_PIXELFORMAT_DIN99DFLOAT32
};
/* Decimal reference on output-native's stored binary32 coordinates gives
 * these P3 percentages in all five spaces; none is a rounding-boundary tie. */
static unsigned char const an_p3[2][3] = {
    { 54, 100, 52 }, { 39, 53, 70 }
};

typedef struct an_capture {
    char data[8192];
    size_t size;
} an_capture_t;

static int an_fail_samples;
static int an_failed_samples;

static void *
an_malloc(size_t size)
{
    /* The fixture has exactly three visible three-channel float samples. */
    if (an_fail_samples && size == 9U * sizeof(float)) {
        an_fail_samples = 0;
        an_failed_samples = 1;
        return NULL;
    }
    return malloc(size);
}

static int
an_write(char *data, int size, void *context)
{
    an_capture_t *out;

    out = context;
    if (size < 0 || (size_t)size >= sizeof(out->data) - out->size) {
        return -1;
    }
    memcpy(out->data + out->size, data, (size_t)size);
    out->size += (size_t)size;
    out->data[out->size] = '\0';
    return size;
}

/* A deliberately narrow independent parser for four pixels on one row.
 * Unpainted slots stay -1, independently of decoder background defaults. */
static int
an_indexes(char const *stream, int result[4])
{
    char const *p;
    int color;
    int x;
    int repeat;
    int code;
    int i;

    for (i = 0; i < 4; ++i) {
        result[i] = -1;
    }
    color = 0;
    x = 0;
    p = strchr(stream, 'q');
    if (p == NULL) {
        return 0;
    }
    ++p;
    while (*p != '\0' && *p != '\033') {
        if (*p == '#') {
            color = 0;
            ++p;
            while (*p >= '0' && *p <= '9') {
                color = color * 10 + *p++ - '0';
                if (color >= SIXEL_PALETTE_MAX) {
                    return 0;
                }
            }
            while ((*p >= '0' && *p <= '9') || *p == ';') {
                ++p;
            }
            continue;
        }
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
        repeat = 1;
        if (*p == '!') {
            repeat = 0;
            ++p;
            while (*p >= '0' && *p <= '9') {
                repeat = repeat * 10 + *p++ - '0';
                if (repeat > 4) {
                    return 0;
                }
            }
        }
        code = *p++ - '?';
        if (code < 0 || code > 1 || repeat < 1 || x + repeat > 4) {
            return 0;
        }
        while (repeat-- > 0) {
            if (code != 0) {
                result[x] = color;
            }
            ++x;
        }
    }
    return 1;
}

static int
test_alpha_native_run(int space, int mode, int animated)
{
    static struct { int flag; char const *value; } const options[] = {
        { '=', "balanced:B1" }, { '~', "none" }, { 'd', "none" },
        { '0', "none" }, { '_', "none" }, { '.', "auto" },
        { 'Q', "kmeans:seed=1" }, { 'F', "none" }, { 'a', "off" },
        { '4', "full-frame" }, { '5', "none" }, { 'p', "3" },
        { 'A', "keep" }
    };
    sixel_encoder_t *encoder;
    sixel_allocator_t *allocator;
    sixel_output_t *output;
    sixel_frame_t *frame;
    sixel_frame_interface_t *frame_if;
    sixel_frame_transparency_t transparency;
    sixel_dither_t *dither;
    sixel_index_t *indexes;
    sixel_palette_float32_entries_view_t view;
    unsigned char const *wire;
    unsigned int count;
    unsigned char *mask;
    unsigned char bytes[12];
    float source[12];
    float saved[12];
    float feedback[9];
    unsigned char feedback_mask[3];
    an_capture_t capture;
    int painted[4];
    int map[2];
    int expected[4];
    int masked;
    int visible_colors;
    int passes;
    int pass;
    int entry;
    int candidate;
    int match;
    int i;
    int j;
    int byte_work;
    int byte_input;
    int ok;
    double want;
    char definition[64];

    encoder = NULL;
    allocator = NULL;
    output = NULL;
    frame = NULL;
    mask = NULL;
    indexes = NULL;
    dither = NULL;
    ok = 0;
    pass = 0;
    byte_input = mode == 7 || mode == 8;
    /* precision-policy=8bit applies even a float source through bytes. */
    byte_work = mode == 7 || mode == 9;
    passes = mode == 11 || mode == 12 ? 3 : 1;
    if (mode == 26 && SIXEL_FAILED(sixel_allocator_new(&allocator,
            an_malloc, NULL, NULL, NULL))) {
        goto end;
    }
    if (SIXEL_FAILED(sixel_encoder_new(&encoder, allocator)) ||
            SIXEL_FAILED(sixel_output_new(&output, an_write, &capture, NULL))) {
        goto end;
    }
    for (i = 0; i < (int)(sizeof(options) / sizeof(options[0])); ++i) {
        if (SIXEL_FAILED(sixel_encoder_setopt(encoder, options[i].flag,
                options[i].value))) {
            goto end;
        }
    }
    if (SIXEL_FAILED(sixel_encoder_setopt(encoder, 'X', an_spaces[space])) ||
            SIXEL_FAILED(sixel_encoder_setopt(encoder,
                'W', an_spaces[space])) ||
            (mode == 1 && SIXEL_FAILED(sixel_encoder_setopt(encoder,
                'U', "display-p3"))) ||
            (mode == 2 && SIXEL_FAILED(sixel_encoder_setopt(encoder,
                'Q', "center:seed=1"))) ||
            (mode == 3 && SIXEL_FAILED(sixel_encoder_setopt(encoder,
                'Q', "medoids:seed=1"))) ||
            (mode == 4 && SIXEL_FAILED(sixel_encoder_setopt(encoder,
                'Q', "heckbert"))) ||
            (mode == 8 && SIXEL_FAILED(sixel_encoder_setopt(encoder,
                '.', "float32"))) ||
            (mode == 9 && SIXEL_FAILED(sixel_encoder_setopt(encoder,
                '.', "8bit"))) ||
            (mode == 10 && SIXEL_FAILED(sixel_encoder_setopt(encoder,
                'A', "clear"))) ||
            ((mode == 15 || mode == 16) &&
             SIXEL_FAILED(sixel_encoder_setopt(encoder,
                '~', "fhedt:resolution=64"))) ||
            (mode == 23 && SIXEL_FAILED(sixel_encoder_setopt(encoder,
                '~', "vptree"))) ||
            (mode == 24 && SIXEL_FAILED(sixel_encoder_setopt(encoder,
                '~', "eytzinger"))) ||
            (mode == 13 && SIXEL_FAILED(sixel_encoder_setopt(encoder,
                'X', "linear"))) ||
            (mode == 17 && SIXEL_FAILED(sixel_encoder_setopt(encoder,
                'B', "#ff0000"))) ||
            (mode >= 20 && mode <= 22 &&
             SIXEL_FAILED(sixel_encoder_setopt(encoder, '5',
                mode == 20 ? "exact" : mode == 21 ? "hard" : "soft")))) {
        goto end;
    }
    if (animated) {
        /* Metadata still denotes an animation. Suppress terminal scrolling
         * in this callback-only test; actual loader tests cover playback. */
        encoder->fstatic = 1;
        encoder->fignore_delay = 1;
        passes = 3;
    }
    for (pass = 0; pass < passes; ++pass) {
        memset(&capture, 0, sizeof(capture));
        memset(&transparency, 0, sizeof(transparency));
        masked = !(mode == 11 && pass == 1);
        visible_colors = mode == 5 || mode == 6 ? 1 : 2;
        if (mode == 11 && SIXEL_FAILED(sixel_encoder_setopt(encoder,
                'p', masked ? "3" : "2"))) {
            goto end;
        }
        for (i = 0; i < 4; ++i) {
            for (j = 0; j < 3; ++j) {
                source[i * 3 + j] = (float)output_native[space]
                    .entries[i == 1 ? 1 : 0][j];
                bytes[i * 3 + j] = i == 1 ? 192 : 64;
            }
            expected[i] = i == 1 ? 1 : 0;
        }
        mask = malloc(4);
        if (mask == NULL) {
            goto end;
        }
        memset(mask, 0, 4);
        if (masked && mode != 18) {
            mask[2] = 1;
            expected[2] = -1;
            for (j = 0; j < 3; ++j) {
                source[6 + j] = mode == 12 && pass == 1 ? -80 : 99;
                bytes[6 + j] = 255;
            }
        }
        if (mode == 5 || mode == 6) {
            memset(mask, 1, 4);
            for (i = 0; i < 4; ++i) {
                expected[i] = -1;
            }
            if (mode == 6) {
                mask[0] = 0;
                expected[0] = 0;
            }
        }
        memcpy(saved, source, sizeof(saved));
        if (SIXEL_FAILED(sixel_frame_new(&frame, NULL)) ||
                SIXEL_FAILED(sixel_frame_init_borrowed(frame,
                    byte_input ? (void *)bytes : (void *)source, 4, 1,
                    byte_input ? SIXEL_PIXELFORMAT_RGB888 : an_formats[space],
                    NULL, -1))) {
            goto end;
        }
        frame_if = sixel_frame_as_interface(frame);
        if (!masked) {
            free(mask);
            mask = NULL;
        }
        transparency.transparent = -1;
        transparency.alpha_zero_is_transparent = masked;
        transparency.transparent_mask = mask;
        transparency.transparent_mask_size = masked ? 4 : 0;
        if (SIXEL_FAILED(frame_if->vtbl->set_transparency(frame_if,
                &transparency))) {
            goto end;
        }
        mask = NULL;
        if (mode == 26) {
            /* A failed pre-build allocation must not install this encoder's
             * seed=1 over a caller's independent override. Retry below with
             * the same encoder/frame after the one-shot allocation fault. */
            sixel_set_kmeans_seed_override(1, 271828U);
            an_fail_samples = 1;
            an_failed_samples = 0;
            if (sixel_encoder_encode_frame(encoder, frame, output) !=
                    SIXEL_BAD_ALLOCATION || !an_failed_samples ||
                    capture.size != 0 ||
                    sixel_get_kmeans_seed() != 271828U) {
                goto end;
            }
            sixel_set_kmeans_seed_override(0, 0U);
        }
        if (animated) {
            sixel_frame_set_multiframe(frame, 1);
            sixel_frame_set_frame_no(frame, pass % 2);
            sixel_frame_set_loop_count(frame, pass / 2);
        }
        if (SIXEL_FAILED(sixel_encoder_encode_frame(encoder, frame, output))) {
            fprintf(stderr, "alpha encode failed: %s\n",
                sixel_helper_get_additional_message());
            goto end;
        }
        if (animated && (encoder->dither_cache != NULL ||
                sixel_frame_get_multiframe(frame) != 1 ||
                sixel_frame_get_frame_no(frame) != pass % 2 ||
                sixel_frame_get_loop_no(frame) != pass / 2)) {
            goto end;
        }
        dither = (sixel_dither_t *)encoder->diagnostic_dither;
        if (dither == NULL || dither->use_rgb101_palette != !byte_work ||
                (!byte_input && memcmp(saved, source, sizeof(saved)) != 0) ||
                !an_indexes(capture.data, painted) ||
                strstr(capture.data, !masked ? "\033Pq" :
                    mode == 10 ? ";0q" : ";1q") == NULL) {
            goto end;
        }
        if (!byte_work) {
            if (dither->ncolors != visible_colors + masked ||
                    (masked && (!dither->keycolor_reserved ||
                     dither->keycolor != visible_colors)) ||
                    SIXEL_FAILED(dither->palette->vtbl->get_entries_float32(
                        dither->palette, &view)) || view.entries == NULL ||
                    SIXEL_FAILED(sixel_palette_get_rgb101(dither->palette,
                        &wire, &count)) || count != view.entry_count) {
                goto end;
            }
            map[0] = -1;
            map[1] = -1;
            for (entry = 0; entry < visible_colors; ++entry) {
                for (candidate = 0; candidate < visible_colors; ++candidate) {
                    match = 1;
                    for (j = 0; j < 3; ++j) {
                        want = mode == 5 ? 0 : byte_input ?
                            (candidate ? 192.0 : 64.0) / 255 :
                            output_native[space].entries[candidate][j];
                        /* Absolute plus relative conversion/storage budget,
                         * well below legacy byte reconstruction error. */
                        if (!isfinite(view.entries[entry * 3 + j]) ||
                                fabs(view.entries[entry * 3 + j] - want) >
                                    2e-6 + fabs(want) * 5e-6) {
                            match = 0;
                        }
                    }
                    if (match) {
                        map[candidate] = entry;
                    }
                }
            }
            if (mode == 17 && (view.entries[visible_colors * 3] != 1 ||
                    view.entries[visible_colors * 3 + 1] != 0 ||
                    view.entries[visible_colors * 3 + 2] != 0)) {
                goto end;
            }
            for (i = 0; i < visible_colors; ++i) {
                if (map[i] < 0) {
                    goto end;
                }
                if (mode != 5 && !byte_input) {
                    if (memcmp(wire + map[i] * 3, mode == 1 ? an_p3[i] :
                            output_native[space].wire[2][i], 3) != 0) {
                        goto end;
                    }
                }
                snprintf(definition, sizeof(definition), "#%d;2;%u;%u;%u",
                    map[i], (unsigned)wire[map[i] * 3],
                    (unsigned)wire[map[i] * 3 + 1],
                    (unsigned)wire[map[i] * 3 + 2]);
                if (mode != 5 && strstr(capture.data, definition) == NULL) {
                    goto end;
                }
            }
            for (i = 0; i < 4; ++i) {
                if (painted[i] != (expected[i] < 0 ? -1 : map[expected[i]])) {
                    goto end;
                }
            }
        } else if (painted[0] < 0 || painted[1] < 0 || painted[2] != -1 ||
                painted[3] < 0) {
            goto end;
        }
        if (mode == 14 || mode == 16 || mode == 23 || mode == 24 ||
                mode == 25) {
            /* On the line A + t(B-A), t=1.125 selects B and sends a
             * residual .125*7/16 to t=.45, which then selects B too.
             * The final pixel is omitted. Replacing FS with NONE changes
             * the second index, and exposing the reserved key breaks the
             * separate black query under the prepared FHEDT lookup. */
            for (i = 0; i < 3; ++i) {
                for (j = 0; j < 3; ++j) {
                    want = i == 0 ? 1.125 : i == 1 ? .45 : 99;
                    feedback[i * 3 + j] = mode != 14 ? 0 :
                        (float)(output_native[space].entries[0][j] + want *
                         (output_native[space].entries[1][j] -
                          output_native[space].entries[0][j]));
                }
                feedback_mask[i] = i == 2;
            }
            sixel_dither_set_diffusion_type(dither, mode == 14 ?
                SIXEL_DIFFUSE_FS : SIXEL_DIFFUSE_NONE);
            sixel_dither_set_diffusion_scan(dither, SIXEL_SCAN_RASTER);
            sixel_dither_set_pipeline_transparent_mask_hint(dither,
                feedback_mask, 3, dither->keycolor);
            indexes = sixel_dither_apply_palette(dither,
                (unsigned char *)(void *)feedback, 3, 1);
            if (indexes == NULL || indexes[0] != map[1] ||
                    indexes[1] != map[1] || indexes[2] != dither->keycolor) {
                goto end;
            }
            sixel_allocator_free(dither->allocator, indexes);
            indexes = NULL;
        }
        sixel_frame_unref(frame);
        frame = NULL;
    }
    ok = 1;
end:
    if (mode == 26) {
        an_fail_samples = 0;
        sixel_set_kmeans_seed_override(0, 0U);
    }
    if (!ok) {
        fprintf(stderr, "native alpha: space=%d mode=%d pass=%d\n",
            space, mode, pass);
    }
    free(mask);
    if (indexes != NULL) {
        sixel_allocator_free(dither->allocator, indexes);
    }
    sixel_frame_unref(frame);
    sixel_output_unref(output);
    sixel_encoder_unref(encoder);
    sixel_allocator_unref(allocator);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}

int
test_alpha_native(int space, int mode)
{
    return test_alpha_native_run(space, mode, 0);
}

int
test_alpha_native_animation(int space, int mode)
{
    return test_alpha_native_run(space, mode, 1);
}
