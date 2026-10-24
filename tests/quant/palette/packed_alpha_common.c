/* SPDX-License-Identifier: MIT */
/* Independent byte-coordinate and wire contracts for packed alpha. */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sixel.h>
#include "src/frame-normalize.h"
#include "src/encoder.h"
#include "src/dither.h"
#include "src/palette.h"

static int const pa_formats[] = {
    SIXEL_PIXELFORMAT_RGBA8888, SIXEL_PIXELFORMAT_ARGB8888,
    SIXEL_PIXELFORMAT_BGRA8888, SIXEL_PIXELFORMAT_ABGR8888,
    SIXEL_PIXELFORMAT_GA88, SIXEL_PIXELFORMAT_AG88
};
static unsigned char const pa_rgb[4][3] = {
    { 17, 91, 203 }, { 201, 73, 19 }, { 5, 151, 227 }, { 43, 99, 177 }
};
static unsigned char const pa_alpha[4] = { 0, 1, 128, 255 };
static unsigned char const pa_background[3] = { 61, 129, 233 };
static int pa_fail_after;

static void *
pa_malloc(size_t size)
{
    if (pa_fail_after > 0 && --pa_fail_after == 0) {
        return NULL;
    }
    return malloc(size);
}

/* Build each external byte layout directly from RGB and coverage literals. */
static void
pa_pack(unsigned char *dst, int format, unsigned char const rgb[4][3],
         unsigned char const *alpha)
{
    int i;

    for (i = 0; i < 4; ++i) {
        switch (format) {
        case 0:
            memcpy(dst + 4 * i, rgb[i], 3);
            dst[4 * i + 3] = alpha[i];
            break;
        case 1:
            dst[4 * i] = alpha[i];
            memcpy(dst + 4 * i + 1, rgb[i], 3);
            break;
        case 2:
            dst[4 * i] = rgb[i][2];
            dst[4 * i + 1] = rgb[i][1];
            dst[4 * i + 2] = rgb[i][0];
            dst[4 * i + 3] = alpha[i];
            break;
        case 3:
            dst[4 * i] = alpha[i];
            dst[4 * i + 1] = rgb[i][2];
            dst[4 * i + 2] = rgb[i][1];
            dst[4 * i + 3] = rgb[i][0];
            break;
        case 4:
            dst[2 * i] = rgb[i][0];
            dst[2 * i + 1] = alpha[i];
            break;
        case 5:
            dst[2 * i] = alpha[i];
            dst[2 * i + 1] = rgb[i][0];
            break;
        }
    }
}

int
test_packed_unit(int format, int mode)
{
    sixel_allocator_t *allocator;
    sixel_frame_t *frame;
    sixel_frame_interface_t *iface;
    sixel_frame_transparency_t transparency;
    sixel_frame_transparency_t before;
    SIXELSTATUS status;
    unsigned char input[16];
    unsigned char saved[16];
    unsigned char alpha[4];
    unsigned char *mask;
    float const *values;
    double want;
    unsigned int foreground;
    unsigned int background;
    int i;
    int c;
    int hidden;
    int ok;

    allocator = NULL;
    frame = NULL;
    mask = NULL;
    ok = 0;
    pa_fail_after = 0;
    memcpy(alpha, pa_alpha, sizeof(alpha));
    if (mode == 3 || mode == 4) {
        alpha[1] = alpha[2] = 255;
    }
    memset(input, 0, sizeof(input));
    pa_pack(input, format, pa_rgb, alpha);
    memcpy(saved, input, sizeof(input));
    if (SIXEL_FAILED(sixel_allocator_new(&allocator, pa_malloc,
            NULL, NULL, NULL)) ||
            SIXEL_FAILED(sixel_frame_new(&frame, allocator)) ||
            SIXEL_FAILED(sixel_frame_init_borrowed(frame, input, 4, 1,
                pa_formats[format], NULL, -1))) {
        goto end;
    }
    iface = sixel_frame_as_interface(frame);
    memset(&transparency, 0, sizeof(transparency));
    transparency.transparent = -1;
    transparency.alpha_zero_is_transparent = mode != 2 && mode != 9;
    if (mode == 5 || mode == 6 || mode == 9) {
        mask = malloc(4);
        if (mask == NULL) {
            goto end;
        }
        memset(mask, 0, 4);
        mask[3] = 1;
        transparency.transparent_mask = mask;
        transparency.transparent_mask_size = mode == 6 ? 3 : 4;
    }
    if (SIXEL_FAILED(iface->vtbl->set_transparency(iface, &transparency))) {
        goto end;
    }
    mask = NULL;
    before = transparency;
    if (mode == 7 || mode == 8) {
        pa_fail_after = mode - 6;
    }
    status = sixel_frame_unpack_alpha(frame,
        mode == 1 ? NULL : pa_background, mode != 3);
    if (mode == 6 || mode == 7 || mode == 8) {
        if (SIXEL_FAILED(iface->vtbl->get_transparency(iface,
                &transparency)) ||
                transparency.transparent_mask != before.transparent_mask ||
                transparency.transparent_mask_size !=
                    before.transparent_mask_size ||
                transparency.alpha_zero_is_transparent !=
                    before.alpha_zero_is_transparent ||
                status != (mode == 6
                    ? SIXEL_BAD_INPUT : SIXEL_BAD_ALLOCATION) ||
                sixel_frame_get_pixels(frame) != input ||
                sixel_frame_get_pixelformat(frame) != pa_formats[format] ||
                memcmp(input, saved, sizeof(input)) != 0) {
            goto end;
        }
        if (mode == 6) {
            ok = 1;
            goto end;
        }
        pa_fail_after = 0;
        status = sixel_frame_unpack_alpha(frame, pa_background, 1);
    }
    if (SIXEL_FAILED(status) || memcmp(input, saved, sizeof(input)) != 0) {
        goto end;
    }
    if (mode == 3) {
        ok = sixel_frame_get_pixelformat(frame) == SIXEL_PIXELFORMAT_RGB888 &&
            memcmp(sixel_frame_get_pixels(frame), pa_rgb, sizeof(pa_rgb)) == 0;
        goto end;
    }
    if (sixel_frame_get_pixelformat(frame) != SIXEL_PIXELFORMAT_RGBFLOAT32 ||
            SIXEL_FAILED(iface->vtbl->get_transparency(iface,
                &transparency))) {
        goto end;
    }
    values = (float const *)sixel_frame_get_pixels(frame);
    for (i = 0; i < 4; ++i) {
        hidden = (mode != 2 && mode != 9 && alpha[i] == 0) ||
            ((mode == 5 || mode == 9) && i == 3);
        if (mode == 2 ? transparency.transparent_mask != NULL :
                transparency.transparent_mask == NULL ||
                transparency.transparent_mask_size != 4 ||
                transparency.transparent_mask[i] != hidden) {
            goto end;
        }
        for (c = 0; c < 3; ++c) {
            foreground = pa_rgb[i][format < 4 ? c : 0];
            /* 121 is the literal legacy gray of the chosen RGB background. */
            background = mode == 1 ? 0 :
                format < 4 ? pa_background[c] : 121;
            want = foreground / 255.0;
            if (mode != 2 && mode != 9 && !hidden && alpha[i] < 255) {
                want = (foreground * (unsigned int)alpha[i] +
                    background * (255u - alpha[i])) / 65025.0;
            }
            /* Only the final float32 store rounds this rational value. */
            if (!isfinite(values[i * 3 + c]) ||
                    fabs(values[i * 3 + c] - want) > 6e-8) {
                fprintf(stderr, "packed unit %d/%d [%d,%d]: %.9g != %.9g\n",
                    format, mode, i, c, values[i * 3 + c], want);
                goto end;
            }
        }
    }
    ok = 1;
end:
    pa_fail_after = 0;
    free(mask);
    sixel_frame_unref(frame);
    sixel_allocator_unref(allocator);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}

typedef struct pa_capture {
    char data[32768];
    size_t size;
} pa_capture_t;

static int
pa_write(char *data, int size, void *context)
{
    pa_capture_t *out;

    out = context;
    if (size < 0 || (size_t)size >= sizeof(out->data) - out->size) {
        return -1;
    }
    memcpy(out->data + out->size, data, (size_t)size);
    out->size += (size_t)size;
    out->data[out->size] = '\0';
    return size;
}

/* Parse the emitted paint/index contract independently of the decoder. */
static int
pa_decode(char const *stream, int width, int result[8], int rgb[256][3])
{
    char const *p;
    int color;
    int x;
    int repeat;
    int code;
    int count;
    int i;

    for (i = 0; i < 8; ++i) {
        result[i] = -1;
    }
    memset(rgb, 0, sizeof(int) * 256 * 3);
    color = 1;
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
                if (color >= 256) {
                    return 0;
                }
            }
            if (*p == ';') {
                count = 0;
                if (sscanf(p, ";2;%d;%d;%d%n", &rgb[color][0],
                        &rgb[color][1], &rgb[color][2], &count) != 3 ||
                        count <= 0) {
                    return 0;
                }
                p += count;
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
        if (*p == '-') {
            ++p;
            if (*p != '\033') {
                return 0;
            }
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
        if (code < 0 || code > 1 || repeat < 1 || x + repeat > width) {
            return 0;
        }
        while (repeat-- > 0) {
            if (code) {
                result[x] = color;
            }
            ++x;
        }
    }
    return 1;
}

static int
test_packed_wire_run(int space, int mode, int animated)
{
    static char const *const spaces[] = {
        "gamma", "linear", "oklab", "cielab", "din99d"
    };
    static struct { int flag; char const *value; } const options[] = {
        { '=', "greedy:B1" }, { '~', "none" }, { 'd', "none" },
        { '0', "none" }, { '_', "none" }, { '.', "float32" },
        { 'Q', "kmeans:seed=1" }, { 'F', "none" }, { 'a', "off" },
        { '4', "full-frame" }, { '5', "none" }, { 'A', "keep" },
        { 't', "rgb" }, { 'p', "5" }
    };
    static int const expected[4][3] = {
        { 7, 36, 80 }, { 0, 0, 0 }, { 1, 30, 45 }, { 17, 39, 69 }
    };
    /* Independent xy-primary matrix solve at Decimal60 precision, with
     * D65 and the standard sRGB transfer on the stored float32 samples. */
    static int const p3[4][3] = {
        { 0, 0, 0 }, { 0, 0, 0 }, { 12, 29, 43 }, { 23, 38, 67 }
    };
    static int const background[4][3] = {
        { 7, 36, 80 }, { 24, 51, 91 }, { 13, 55, 90 }, { 17, 39, 69 }
    };
    static int const unblended[4][3] = {
        { 7, 36, 80 }, { 79, 29, 7 }, { 2, 59, 89 }, { 17, 39, 69 }
    };
    static int const fs[4] = { -1, 0, 1, 0 };
    sixel_allocator_t *allocator;
    SIXELSTATUS status;
    sixel_encoder_t *encoder;
    sixel_output_t *output;
    sixel_frame_t *frame;
    sixel_frame_interface_t *iface;
    sixel_frame_transparency_t transparency;
    sixel_dither_t *dither;
    sixel_palette_float32_entries_view_t view;
    pa_capture_t capture;
    pa_capture_t first;
    unsigned char source_rgb[4][3];
    unsigned char alpha[4];
    unsigned char input[16];
    unsigned char saved[16];
    int painted[8];
    int colors[256][3];
    int wanted;
    int hidden;
    int source_x;
    int width;
    int passes;
    int pass;
    int i;
    int c;
    int ok;
    int interpret;
    int found;
    double want;
    unsigned int entry;
    unsigned int n;

    allocator = NULL;
    encoder = NULL;
    output = NULL;
    frame = NULL;
    ok = 0;
    width = mode == 7 ? 3 : mode == 8 ? 8 : 4;
    passes = mode == 9 ? 3 : mode == 11 ? 2 : 1;
    if (mode == 15 && SIXEL_FAILED(sixel_allocator_new(&allocator,
            pa_malloc, NULL, NULL, NULL))) {
        return EXIT_FAILURE;
    }
    memset(&capture, 0, sizeof(capture));
    memset(&first, 0, sizeof(first));
    if (SIXEL_FAILED(sixel_encoder_new(&encoder, NULL)) ||
            SIXEL_FAILED(sixel_output_new(&output, pa_write, &capture, NULL))) {
        goto end;
    }
    for (n = 0; n < sizeof(options) / sizeof(options[0]); ++n) {
        if ((mode == 6 || mode == 14) &&
                (options[n].flag == 'a' || options[n].flag == 'p')) {
            continue;
        }
        if (SIXEL_FAILED(sixel_encoder_setopt(encoder,
                options[n].flag, options[n].value))) {
            goto end;
        }
    }
    if (SIXEL_FAILED(sixel_encoder_setopt(encoder, 'X', spaces[space])) ||
            SIXEL_FAILED(sixel_encoder_setopt(encoder, 'W', spaces[space])) ||
            (mode == 1 && SIXEL_FAILED(sixel_encoder_setopt(encoder,
                'U', "display-p3"))) ||
            (mode == 2 && SIXEL_FAILED(sixel_encoder_setopt(encoder,
                'B', "rgb:3d/81/e9"))) ||
            (mode == 3 && SIXEL_FAILED(sixel_encoder_setopt(encoder,
                'A', "clear"))) ||
            ((mode == 4 || mode == 13) &&
             SIXEL_FAILED(sixel_encoder_setopt(encoder, '.', "8bit"))) ||
            ((mode == 5 || mode == 11) &&
             SIXEL_FAILED(sixel_encoder_setopt(encoder, 'a',
                 mode == 5 ? "corners:Whard:V1" : "corners:Wsoft:V1"))) ||
            ((mode == 6 || mode == 14) &&
             SIXEL_FAILED(sixel_encoder_setopt(encoder, 'b', "gray1"))) ||
            (mode == 7 && SIXEL_FAILED(sixel_encoder_setopt(encoder,
                'c', "3x1+1+0"))) ||
            (mode == 8 && (SIXEL_FAILED(sixel_encoder_setopt(encoder,
                'w', "8")) || SIXEL_FAILED(sixel_encoder_setopt(encoder,
                'h', "1")) || SIXEL_FAILED(sixel_encoder_setopt(encoder,
                'r', "nearest")))) ||
            (mode == 14 && SIXEL_FAILED(sixel_encoder_setopt(encoder,
                'd', "fs")))) {
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
        if (animated && mode == 8) {
            width = pass == 1 ? 4 : 8;
            if (SIXEL_FAILED(sixel_encoder_setopt(encoder, 'w',
                    width == 4 ? "4" : "8"))) {
                goto end;
            }
        }
        memcpy(source_rgb, pa_rgb, sizeof(source_rgb));
        memcpy(alpha, pa_alpha, sizeof(alpha));
        interpret = mode != 12 && !(mode == 9 && pass == 1);
        if (mode == 4) {
            alpha[1] = alpha[2] = 255;
        }
        if (mode == 6 || mode == 14) {
            memset(source_rgb, mode == 6 ? 255 : 204, sizeof(source_rgb));
            alpha[1] = alpha[2] = alpha[3] = mode == 6 ? 255 : 128;
        }
        if (mode == 10) {
            memset(alpha, 0, sizeof(alpha));
        }
        if (mode == 11 && pass == 1) {
            memset(source_rgb[0], 255, 3);
        }
        /* C99 needs an explicit read-only view of this mutable array. */
        pa_pack(input, 0, (unsigned char const (*)[3])source_rgb, alpha);
        memcpy(saved, input, sizeof(input));
        memset(&capture, 0, sizeof(capture));
        memset(&transparency, 0, sizeof(transparency));
        if (SIXEL_FAILED(sixel_frame_new(&frame, allocator)) ||
                SIXEL_FAILED(sixel_frame_init_borrowed(frame,
                    input, 4, 1, SIXEL_PIXELFORMAT_RGBA8888, NULL, -1))) {
            goto end;
        }
        transparency.transparent = -1;
        transparency.alpha_zero_is_transparent = interpret;
        iface = sixel_frame_as_interface(frame);
        if (SIXEL_FAILED(iface->vtbl->set_transparency(iface,
                &transparency))) {
            goto end;
        }
        if (mode == 15) {
            pa_fail_after = 1;
            status = sixel_encoder_encode_frame(encoder, frame, output);
            pa_fail_after = 0;
            if (status != SIXEL_BAD_ALLOCATION || capture.size != 0 ||
                    sixel_frame_get_pixels(frame) != input ||
                    sixel_frame_get_pixelformat(frame) !=
                        SIXEL_PIXELFORMAT_RGBA8888) {
                goto end;
            }
        }
        if (animated) {
            sixel_frame_set_multiframe(frame, 1);
            sixel_frame_set_frame_no(frame, pass % 2);
            sixel_frame_set_loop_count(frame, pass / 2);
        }
        if (SIXEL_FAILED(sixel_encoder_encode_frame(encoder, frame, output))) {
            goto end;
        }
        if (animated && (encoder->dither_cache != NULL ||
                sixel_frame_get_multiframe(frame) != 1 ||
                sixel_frame_get_frame_no(frame) != pass % 2 ||
                sixel_frame_get_loop_no(frame) != pass / 2)) {
            goto end;
        }
        dither = (sixel_dither_t *)encoder->diagnostic_dither;
        if (dither == NULL ||
                dither->use_rgb101_palette != (mode != 4 && mode != 13) ||
                (mode == 4 && dither->ncolors != 4) ||
                memcmp(input, saved, sizeof(input)) != 0 ||
                !pa_decode(capture.data, width, painted, colors)) {
            goto end;
        }
        /* precision-policy=8bit composes in float, then applies bytes. */
        if (mode == 13 && (SIXEL_FAILED(
                dither->palette->vtbl->get_entries_float32(
                    dither->palette, &view)) || view.entries != NULL)) {
            goto end;
        }
        /* The wire grid cannot expose every intermediate byte rounding.
         * Require the actual gamma palette to retain each rational blend. */
        if (space == 0 && mode == 0) {
            if (SIXEL_FAILED(dither->palette->vtbl->get_entries_float32(
                    dither->palette, &view)) || view.entries == NULL) {
                goto end;
            }
            for (i = 1; i < 4; ++i) {
                found = 0;
                for (entry = 0; entry < view.entry_count; ++entry) {
                    for (c = 0; c < 3; ++c) {
                        want = pa_rgb[i][c] * (unsigned int)pa_alpha[i]
                            / 65025.0;
                        if (!isfinite(view.entries[entry * 3u + c]) ||
                                fabs(view.entries[entry * 3u + c] - want)
                                    > 6e-8) {
                            break;
                        }
                    }
                    found |= c == 3;
                }
                if (!found) {
                    goto end;
                }
            }
        }
        if (interpret && mode != 7 &&
                strstr(capture.data, mode == 3 ? "\033P0;0q" :
                    "\033P0;1q") == NULL) {
            goto end;
        }
        for (i = 0; i < width; ++i) {
            source_x = mode == 7 ? i + 1 :
                mode == 8 && width == 8 ? i / 2 : i;
            hidden = interpret && (source_x == 0 || mode == 10);
            if (hidden ? painted[i] != -1 : painted[i] < 0) {
                goto end;
            }
            if (hidden) {
                continue;
            }
            if (mode == 14) {
                if (painted[i] != fs[source_x]) {
                    goto end;
                }
                continue;
            }
            for (c = 0; c < 3; ++c) {
                wanted = mode == 6 ? 100 : mode == 1 ? p3[source_x][c] :
                    mode == 2 ? background[source_x][c] :
                    !interpret || mode == 4 ? unblended[source_x][c] :
                    expected[source_x][c];
                if (colors[painted[i]][c] != wanted) {
                    fprintf(stderr, "packed wire %d/%d [%d,%d]: %d != %d\n",
                        space, mode, i, c, colors[painted[i]][c], wanted);
                    goto end;
                }
            }
        }
        if (pass == 0) {
            first = capture;
        } else if ((mode == 11 || pass == 2) &&
                (capture.size != first.size ||
                 memcmp(capture.data, first.data, capture.size) != 0)) {
            goto end;
        }
        sixel_frame_unref(frame);
        frame = NULL;
    }
    ok = 1;
end:
    if (!ok) {
        fprintf(stderr, "packed wire space=%d mode=%d: %s\n%s\n",
            space, mode, sixel_helper_get_additional_message(), capture.data);
    }
    sixel_frame_unref(frame);
    sixel_output_unref(output);
    sixel_encoder_unref(encoder);
    sixel_allocator_unref(allocator);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}

int
test_packed_wire(int space, int mode)
{
    return test_packed_wire_run(space, mode, 0);
}

int
test_packed_wire_animation(int space, int mode)
{
    return test_packed_wire_run(space, mode, 1);
}
