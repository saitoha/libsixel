/* SPDX-License-Identifier: MIT */
/* Fixed entries and omission masks have independent index domains. */
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
#include "tests/data/color-numeric/builtin-native.h"

double const *test_map_p3_primary(int space);

static char const *const fm_spaces[] = {
    "gamma", "linear", "oklab", "cielab", "din99d"
};
static int const fm_formats[] = {
    SIXEL_PIXELFORMAT_RGBFLOAT32, SIXEL_PIXELFORMAT_LINEARRGBFLOAT32,
    SIXEL_PIXELFORMAT_OKLABFLOAT32, SIXEL_PIXELFORMAT_CIELABFLOAT32,
    SIXEL_PIXELFORMAT_DIN99DFLOAT32
};

typedef struct fm_capture {
    char data[65536];
    size_t size;
} fm_capture_t;

static int
fm_write(char *data, int size, void *context)
{
    fm_capture_t *out;

    out = context;
    if (size < 0 || (size_t)size >= sizeof(out->data) - out->size) {
        return -1;
    }
    memcpy(out->data + out->size, data, (size_t)size);
    out->size += (size_t)size;
    out->data[out->size] = '\0';
    return size;
}

/* Parse only the emitted normal SIXEL subset, independently of the library
 * decoder. Preserve holes as -1, rather than inventing a background index.
 * Bounds include the wire margins, so a shifted mask cannot pass unnoticed. */
static int
fm_painted(char const *stream, int *result, int width, int height)
{
    char const *p;
    int color;
    int x;
    int y;
    int repeat;
    int code;
    int bit;
    int i;

    for (i = 0; i < width * height; ++i) {
        result[i] = -1;
    }
    color = 1; /* Palette-free monochrome draws the foreground. */
    x = 0;
    y = 0;
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
        if (*p == '$' || *p == '-') {
            x = 0;
            if (*p == '-') {
                y += 6;
            }
            ++p;
            continue;
        }
        repeat = 1;
        if (*p == '!') {
            repeat = 0;
            ++p;
            while (*p >= '0' && *p <= '9') {
                repeat = repeat * 10 + *p++ - '0';
                if (repeat > width) {
                    return 0;
                }
            }
        }
        code = *p++ - '?';
        if (code < 0 || code > 63 || repeat < 1 || x + repeat > width) {
            return 0;
        }
        while (repeat-- > 0) {
            for (bit = 0; bit < 6; ++bit) {
                if ((code & (1 << bit)) != 0) {
                    if (y + bit >= height) {
                        return 0;
                    }
                    result[(y + bit) * width + x] = color;
                }
            }
            ++x;
        }
    }
    return 1;
}

static int
test_fixed_mask_run(int space, int mode, char const *path, int animated)
{
    static struct { int flag; char const *value; } const options[] = {
        { '=', "1" }, { '~', "none" }, { 'd', "none" },
        { '0', "none" }, { '_', "none" }, { '.', "auto" },
        { 'A', "keep" }, { 'E', "size" }
    };
    static unsigned char const map_rgb[4][3] = {
        { 88, 88, 88 }, { 94, 94, 94 }, { 88, 88, 88 }, { 255, 0, 0 }
    };
    static unsigned char const map_p3[4][3] = {
        { 35, 35, 35 }, { 37, 37, 37 }, { 35, 35, 35 }, { 92, 20, 14 }
    };
    static float const fs_source[4] = { .4f, .4f, 99.f, .49f };
    static int const fs_indexes[4] = { 0, 1, -1, 0 };
    sixel_encoder_t *encoder;
    sixel_output_t *output;
    sixel_frame_t *frame;
    sixel_frame_interface_t *frame_if;
    sixel_frame_transparency_t transparency;
    sixel_palette_float32_entries_view_t view;
    sixel_dither_t *dither;
    unsigned char const *wire;
    unsigned int wire_count;
    unsigned char *mask;
    unsigned char bytes[257 * 13 * 3];
    float source[257 * 13 * 3];
    float saved[257 * 13 * 3];
    int expected[260 * 18];
    int painted[260 * 18];
    fm_capture_t capture;
    int width;
    int height;
    int count;
    int full_palette;
    int wide_map;
    int primary;
    int offset_x;
    int offset_y;
    int ew;
    int eh;
    int p3;
    int mono;
    int byte_work;
    int byte_input;
    int table;
    int masked;
    int passes;
    int pass;
    int i;
    int j;
    int entry;
    int slot;
    int index;
    int ok;
    double want;
    unsigned char want_wire[3];
    char definition[64];

    encoder = NULL;
    output = NULL;
    frame = NULL;
    mask = NULL;
    ok = 0;
    pass = 0;
    full_palette = mode == 2 || mode == 21;
    wide_map = mode == 17 || mode == 18;
    primary = -1;
    p3 = mode == 1 || mode == 14 || mode == 17 || mode == 18;
    mono = mode == 15 || mode == 16;
    byte_input = mode == 11 || mode == 12 || mode == 18;
    byte_work = mode == 11;
    table = mode == 3 || mono ? 3 : 0;
    count = full_palette ? 256 : path != NULL ? 4 :
        (int)builtin_native[table].count;
    width = mode == 3 ? 4 : count + 1;
    height = mode == 3 ? 1 : full_palette ? 13 : 7;
    offset_x = mode == 6 ? 3 : 0;
    offset_y = mode == 6 ? 5 : 0;
    ew = width + offset_x;
    eh = height + offset_y;
    passes = mode == 7 ? 3 : 1;
    memset(&capture, 0, sizeof(capture));
    if (SIXEL_FAILED(sixel_encoder_new(&encoder, NULL)) ||
            SIXEL_FAILED(sixel_output_new(&output, fm_write, &capture, NULL))) {
        goto end;
    }
    for (i = 0; i < (int)(sizeof(options) / sizeof(options[0])); ++i) {
        if (SIXEL_FAILED(sixel_encoder_setopt(encoder, options[i].flag,
                options[i].value))) {
            goto end;
        }
    }
    if (((mode == 5 || mode == 6 || mode == 21) &&
            SIXEL_FAILED(sixel_encoder_setopt(encoder, '=', "4"))) ||
            SIXEL_FAILED(sixel_encoder_setopt(encoder,
                'W', fm_spaces[space])) ||
            SIXEL_FAILED(sixel_encoder_setopt(encoder, mono ? 'e' :
                path != NULL ? 'm' : 'b', mono ? NULL : path != NULL ?
                path : full_palette ? "gray8" : table == 3 ? "gray1" :
                "xterm16")) ||
            (mode == 16 && SIXEL_FAILED(sixel_encoder_setopt(encoder,
                'i', NULL))) ||
            (p3 && SIXEL_FAILED(sixel_encoder_setopt(encoder,
                'U', "display-p3"))) ||
            (mode == 3 && SIXEL_FAILED(sixel_encoder_setopt(encoder,
                'd', "fs"))) ||
            (mode == 4 && SIXEL_FAILED(sixel_encoder_setopt(encoder,
                '~', "fhedt:resolution=64"))) ||
            (mode == 10 && SIXEL_FAILED(sixel_encoder_setopt(encoder,
                'A', "clear"))) ||
            (mode == 12 && SIXEL_FAILED(sixel_encoder_setopt(encoder,
                '.', "float32"))) ||
            ((mode == 17 || mode == 18) &&
             SIXEL_FAILED(sixel_encoder_setopt(encoder, 'L',
                "builtin:cms_engine=builtin:cms_target=linear:"
                "prefer_8bit=0!")))) {
        goto end;
    }
    encoder->transparent_offset_left = offset_x;
    encoder->transparent_offset_top = offset_y;
    if (animated) {
        passes = 3;
        encoder->fstatic = 1;
        encoder->fignore_delay = 1;
    }
    for (pass = 0; pass < passes; ++pass) {
        memset(&capture, 0, sizeof(capture));
        memset(&transparency, 0, sizeof(transparency));
        masked = !byte_work && !(mode == 7 && pass == 1);
        mask = malloc((size_t)width * (size_t)height);
        if (mask == NULL) {
            goto end;
        }
        for (i = 0; i < ew * eh; ++i) {
            expected[i] = -1;
        }
        for (i = 0; i < width * height; ++i) {
            entry = i % width % count;
            index = (i / width + offset_y) * ew + i % width + offset_x;
            mask[i] = masked && mode != 9 &&
                (mode == 8 || i % width == count ||
                 (i % width == 7 && (i / width) % 2 != 0));
            if (mode == 3) {
                mask[i] = i == 2;
            }
            expected[index] = mask[i] ? -1 : mode == 3 ? fs_indexes[i] :
                mono && entry == 0 ? -1 :
                path != NULL && entry == 2 ? 0 : entry;
            for (j = 0; j < 3; ++j) {
                want = mode == 17 || mode == 18 ?
                    test_map_p3_primary(space)[j] :
                    full_palette ? entry / 255.0 : path != NULL ?
                    map_rgb[entry][j] / 255.0 :
                    builtin_native[table].entries[space]
                        [mode == 16 ? 1 - entry : entry][j];
                source[i * 3 + j] = mode == 3 ? fs_source[i] :
                    mask[i] ? (j == 1 ? -99.f : 99.f) : (float)want;
                bytes[i * 3 + j] = mode == 18 ? (j == 0 ? 255 : 0) :
                    byte_input ? builtin_native[table].rgb[entry][j] : 0;
            }
        }
        memcpy(saved, source, (size_t)width * height * 3 * sizeof(float));
        if (SIXEL_FAILED(sixel_frame_new(&frame, NULL)) ||
                SIXEL_FAILED(sixel_frame_init_borrowed(frame,
                    byte_input ? (void *)bytes : (void *)source, width,
                    height, byte_input ? SIXEL_PIXELFORMAT_RGB888 :
                    fm_formats[space], NULL, -1))) {
            goto end;
        }
        if (animated) {
            sixel_frame_set_multiframe(frame, 1);
            sixel_frame_set_frame_no(frame, pass % 2);
            sixel_frame_set_loop_count(frame, pass / 2);
        }
        frame_if = sixel_frame_as_interface(frame);
        if (!masked) {
            free(mask);
            mask = NULL;
        }
        transparency.transparent = -1;
        transparency.alpha_zero_is_transparent = masked;
        transparency.transparent_mask = mask;
        transparency.transparent_mask_size = masked ? width * height : 0;
        if (SIXEL_FAILED(frame_if->vtbl->set_transparency(frame_if,
                &transparency))) {
            goto end;
        }
        mask = NULL;
        if (SIXEL_FAILED(sixel_encoder_encode_frame(encoder, frame, output))) {
            goto end;
        }
        dither = (sixel_dither_t *)encoder->diagnostic_dither;
        if (dither == NULL || dither->use_rgb101_palette != !byte_work ||
                (!byte_input && memcmp(saved, source,
                    (size_t)width * height * 3 * sizeof(float)) != 0) ||
                !fm_painted(capture.data, painted, ew, eh) ||
                strstr(capture.data, !masked && !mono ? "\033Pq" :
                    mode == 10 ? ";0q" : ";1q") == NULL) {
            goto end;
        }
        if (wide_map) {
            if (SIXEL_FAILED(dither->palette->vtbl->get_entries_float32(
                    dither->palette, &view)) || view.entries == NULL) {
                goto end;
            }
            for (entry = 0; entry < (int)view.entry_count; ++entry) {
                for (j = 0; j < 3; ++j) {
                    /* The image map's ICC matrix has fixed-point entries;
                     * 3e-4 covers profile quantization, never gamut clipping.
                     */
                    if (!isfinite(view.entries[entry * 3 + j]) ||
                            fabs(view.entries[entry * 3 + j] -
                                test_map_p3_primary(space)[j]) > 3e-4) {
                        break;
                    }
                }
                if (j == 3) {
                    primary = entry;
                }
            }
            if (primary < 0) {
                goto end;
            }
            for (i = 0; i < ew * eh; ++i) {
                if (expected[i] >= 0) {
                    expected[i] = primary;
                }
            }
        }
#if SIXEL_ENABLE_THREADS
        if ((mode == 5 || mode == 6 || mode == 21) &&
                dither->pipeline_last_encode_threads < 2) {
            fprintf(stderr, "parallel writer was not exercised\n");
            goto end;
        }
#endif
        for (i = 0; i < ew * eh; ++i) {
            if (painted[i] != expected[i]) {
                fprintf(stderr, "paint %d: %d != %d\n", i,
                    painted[i], expected[i]);
                goto end;
            }
        }
        if (!byte_work) {
            if (dither->ncolors != count || dither->keycolor_reserved ||
                    dither->keycolor != (mono ? 0 : -1) ||
                    SIXEL_FAILED(dither->palette->vtbl->get_entries_float32(
                        dither->palette, &view)) || view.entries == NULL ||
                    view.entry_count != (unsigned)count ||
                    SIXEL_FAILED(sixel_palette_get_rgb101(dither->palette,
                        &wire, &wire_count)) || wire_count != (unsigned)count) {
                goto end;
            }
            for (entry = 0; entry < count; ++entry) {
                if (wide_map && entry != primary) {
                    continue;
                }
                slot = mode == 16 ? 1 - entry : entry;
                for (j = 0; j < 3; ++j) {
                    want = mode == 17 || mode == 18 ?
                        test_map_p3_primary(space)[j] :
                        full_palette ? entry / 255.0 : path != NULL ?
                        map_rgb[entry][j] / 255.0 :
                        builtin_native[table].entries[space][slot][j];
                    /* Independent reference plus conversion/storage budget;
                     * opponent-byte reconstruction exceeds this tolerance. */
                    if (!isfinite(view.entries[entry * 3 + j]) ||
                            fabs(view.entries[entry * 3 + j] - want) >
                                (wide_map ? 3e-4 :
                                    2e-6 + fabs(want) * 5e-6)) {
                        goto end;
                    }
                    want_wire[j] = mode == 17 || mode == 18 ?
                        (j == 0 ? 100 : 0) : full_palette ?
                        (entry * 100 + 127) / 255 :
                        path != NULL ? p3 ? map_p3[entry][j] :
                        (map_rgb[entry][j] * 100 + 127) / 255 :
                        builtin_native[table].wire[space][p3][slot][j];
                }
                if (memcmp(wire + entry * 3, want_wire, 3) != 0) {
                    goto end;
                }
                snprintf(definition, sizeof(definition), "#%d;2;%u;%u;%u",
                    entry, (unsigned)want_wire[0],
                    (unsigned)want_wire[1], (unsigned)want_wire[2]);
                if (!mono && strstr(capture.data, definition) == NULL) {
                    goto end;
                }
            }
            if (mono && strchr(capture.data, '#') != NULL) {
                goto end;
            }
        }
        sixel_frame_unref(frame);
        frame = NULL;
    }
    ok = 1;
end:
    if (!ok) {
        fprintf(stderr, "fixed mask space=%d mode=%d pass=%d: %s\n",
            space, mode, pass, sixel_helper_get_additional_message());
    }
    free(mask);
    sixel_frame_unref(frame);
    sixel_output_unref(output);
    sixel_encoder_unref(encoder);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}

/* Reject incomplete/unsupported output masks before writing, then retry on
 * the same handles. This uses the private borrowed-mask boundary directly. */
int
test_fixed_mask_invalid(int mode)
{
    sixel_dither_t *dither;
    sixel_output_t *output;
    unsigned char pixels[6] = { 0, 0, 0, 255, 255, 255 };
    unsigned char palette[6] = { 0, 0, 0, 255, 255, 255 };
    unsigned char mask[2] = { 1, 0 };
    fm_capture_t capture;
    int painted[2];
    int pass;
    int ok;

    dither = NULL;
    output = NULL;
    ok = 0;
    memset(&capture, 0, sizeof(capture));
    if (SIXEL_FAILED(sixel_dither_new(&dither, 2, NULL)) ||
            SIXEL_FAILED(sixel_output_new(&output, fm_write, &capture, NULL))) {
        goto end;
    }
    sixel_dither_set_palette(dither, palette);
    sixel_dither_set_pixelformat(dither, SIXEL_PIXELFORMAT_RGB888);
    sixel_dither_set_diffusion_type(dither, SIXEL_DIFFUSE_NONE);
    sixel_dither_set_optimize_palette(dither, 0);
    dither->pipeline_transparent_mask = mask;
    dither->pipeline_transparent_mask_size = mode == 0 ? 1 : 2;
    dither->pipeline_transparent_keycolor = 0;
    sixel_output_set_ormode(output, mode != 0);
    if (sixel_encode(pixels, 2, 1, 3, dither, output) != SIXEL_BAD_ARGUMENT ||
            capture.size != 0) {
        goto end;
    }
    sixel_output_set_ormode(output, 0);
    for (pass = 0; pass < 2; ++pass) {
        memset(&capture, 0, sizeof(capture));
        dither->pipeline_transparent_mask = pass == 0 ? mask : NULL;
        dither->pipeline_transparent_mask_size = pass == 0 ? 2 : 0;
        dither->pipeline_transparent_keycolor = pass == 0 ? 0 : -1;
        if (SIXEL_FAILED(sixel_encode(pixels, 2, 1, 3, dither, output)) ||
                !fm_painted(capture.data, painted, 2, 1) ||
                painted[0] != (pass == 0 ? -1 : 0) || painted[1] != 1) {
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
test_fixed_mask(int space, int mode, char const *path)
{
    return test_fixed_mask_run(space, mode, path, 0);
}

int
test_fixed_mask_animation(int space, int mode, char const *path)
{
    return test_fixed_mask_run(space, mode, path, 1);
}
