/* SPDX-License-Identifier: MIT */
/* Cover owns real colors; omission slots belong to the caller. */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sixel.h>
#include "src/encoder.h"
#include "src/dither.h"
#include "src/palette.h"
#include "src/palette-common-cover.h"

/* Literal gamma coordinates avoid transform-dependent expected values. */
static int
cm_capacity(unsigned int capacity, unsigned int initial, int grow, int hard)
{
    sixel_allocator_t *allocator;
    SIXELSTATUS status;
    float entries[256 * 3];
    float before[256 * 3];
    float samples[6] = { 2, 0, 0, 2, 0, 0 };
    unsigned int count;
    unsigned int expected;
    unsigned int i;
    unsigned int j;
    unsigned int anchor;
    int found;
    int ok;

    allocator = NULL;
    ok = 0;
    count = initial;
    for (i = 0; i < 256 * 3; ++i) {
        entries[i] = i < initial * 3 ? .5f : 7.f;
    }
    memcpy(before, entries, sizeof(before));
    if (SIXEL_FAILED(sixel_allocator_new(&allocator, NULL, NULL,
            NULL, NULL))) {
        goto end;
    }
    status = sixel_palette_cover_float(entries, &count, samples,
        sizeof(samples), SIXEL_PIXELFORMAT_RGBFLOAT32,
        SIXEL_PALETTE_COVER_CORNERS, hard ? SIXEL_PALETTE_COVER_MODE_HARD :
        SIXEL_PALETTE_COVER_MODE_SOFT, grow, capacity, allocator);
    if (capacity == 0 || initial > capacity || capacity > 256) {
        ok = status == SIXEL_BAD_ARGUMENT && count == initial &&
            memcmp(before, entries, sizeof(before)) == 0;
        goto end;
    }
    expected = grow ? capacity : initial;
    if (SIXEL_FAILED(status) || count != expected ||
            memcmp(entries + expected * 3, before + expected * 3,
                (256 - expected) * 3 * sizeof(float)) != 0) {
        goto end;
    }
    /* At the ceiling, merge-funded placement must still place every target.
     * These literals also reject accidental clipping of the soft anchor. */
    for (anchor = 0; anchor < (hard ? 8U : 1U); ++anchor) {
        found = 0;
        for (i = 0; i < count; ++i) {
            for (j = 0; j < 3; ++j) {
                if (entries[i * 3 + j] != (hard ?
                        (float)((anchor >> j) & 1U) : j == 0 ? 2.f : 0.f)) {
                    break;
                }
            }
            found |= j == 3;
        }
        if (!found) {
            goto end;
        }
    }
    ok = 1;
end:
    sixel_allocator_unref(allocator);
    return ok;
}

typedef struct cm_capture {
    char data[65536];
    size_t size;
} cm_capture_t;

static int
cm_write(char *data, int size, void *context)
{
    cm_capture_t *out;

    out = context;
    if (size < 0 || (size_t)size >= sizeof(out->data) - out->size) {
        return -1;
    }
    memcpy(out->data + out->size, data, (size_t)size);
    out->size += (size_t)size;
    out->data[out->size] = '\0';
    return size;
}

/* Independent normal-SIXEL paint parser: holes retain -1. */
static int
cm_painted(char const *stream, int *result, int width, int height)
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
cm_wire(int mode)
{
    static struct { int flag; char const *value; } const options[] = {
        { '=', "1" }, { '~', "none" }, { 'd', "none" },
        { '0', "none" }, { '_', "none" }, { '.', "auto" },
        { 'Q', "kmeans:seed=1" }, { 'F', "none" },
        { '4', "full-frame" }, { '5', "none" }, { 'A', "keep" },
        { 'X', "gamma" }, { 'W', "gamma" }, { 't', "rgb" }
    };
    sixel_encoder_t *encoder;
    sixel_output_t *output;
    sixel_frame_t *frame;
    sixel_frame_interface_t *iface;
    sixel_frame_transparency_t transparency;
    sixel_palette_float32_entries_view_t view;
    sixel_dither_t *dither;
    float pixels[255 * 13 * 3];
    unsigned char bytes[255 * 13 * 3];
    unsigned char *mask;
    int painted[255 * 13];
    cm_capture_t capture;
    cm_capture_t first;
    int boundary;
    int hard;
    int grow;
    int byte;
    int masked;
    int width;
    int height;
    int pass;
    int passes;
    int i;
    int j;
    int x;
    int hidden;
    int ok;
    unsigned int n;
    float want;

    encoder = NULL;
    output = NULL;
    frame = NULL;
    mask = NULL;
    ok = 0;
    boundary = mode == 6 || mode == 7 || mode == 8 || mode == 11;
    hard = mode == 1 || mode == 3 || mode == 5 || boundary ||
        mode == 9 || mode == 12;
    grow = mode != 2 && mode != 3;
    byte = mode == 11;
    width = boundary ? 255 : 7;
    height = mode == 12 ? 13 : 1;
    passes = mode == 8 ? 3 : mode == 10 ? 2 : 1;
    memset(&first, 0, sizeof(first));
    memset(&capture, 0, sizeof(capture));
    if (SIXEL_FAILED(sixel_encoder_new(&encoder, NULL)) ||
            SIXEL_FAILED(sixel_output_new(&output, cm_write,
                &capture, NULL))) {
        goto end;
    }
    for (n = 0; n < sizeof(options) / sizeof(options[0]); ++n) {
        if (SIXEL_FAILED(sixel_encoder_setopt(encoder,
                options[n].flag, options[n].value))) {
            goto end;
        }
    }
    if (SIXEL_FAILED(sixel_encoder_setopt(encoder, 'p',
            boundary ? "256" : "3")) ||
            SIXEL_FAILED(sixel_encoder_setopt(encoder, 'a', mode >= 13 ?
                "corners:V1" : hard ?
                (grow ? "corners:Whard:V1" : "corners:Whard:V0") :
                (grow ? "corners:Wsoft:V1" : "corners:Wsoft:V0"))) ||
            ((mode == 4 || mode == 5 || mode == 14) &&
             SIXEL_FAILED(sixel_encoder_setopt(encoder,
                'U', "display-p3"))) ||
            (mode == 12 && SIXEL_FAILED(sixel_encoder_setopt(encoder,
                '=', "4")))) {
        goto end;
    }
    for (pass = 0; pass < passes; ++pass) {
        masked = mode != 7 && !(mode == 8 && pass == 1);
        memset(&capture, 0, sizeof(capture));
        memset(&transparency, 0, sizeof(transparency));
        mask = masked ? malloc((size_t)width * height) : NULL;
        if (masked && mask == NULL) {
            goto end;
        }
        for (i = 0; i < width * height; ++i) {
            x = i % width;
            hidden = masked && (x == width - 1 || mode == 9);
            if (mask != NULL) {
                mask[i] = (unsigned char)hidden;
            }
            for (j = 0; j < 3; ++j) {
                bytes[i * 3 + j] = (unsigned char)(16 + 32 *
                    ((x % 254 >> (j * 3)) & 7));
                want = boundary ? bytes[i * 3 + j] / 255.f :
                    x < 2 ? 0.f : x < 4 ? .5f : 1.5f;
                pixels[i * 3 + j] = hidden ?
                    (pass == 1 ? -200.f : 200.f) : want;
            }
        }
        if (SIXEL_FAILED(sixel_frame_new(&frame, NULL)) ||
                SIXEL_FAILED(sixel_frame_init_borrowed(frame,
                    byte ? (void *)bytes : (void *)pixels,
                    width, height, byte ? SIXEL_PIXELFORMAT_RGB888 :
                    SIXEL_PIXELFORMAT_RGBFLOAT32, NULL, -1))) {
            goto end;
        }
        iface = sixel_frame_as_interface(frame);
        transparency.transparent = -1;
        transparency.alpha_zero_is_transparent = masked;
        transparency.transparent_mask = mask;
        transparency.transparent_mask_size = masked ?
            (size_t)width * height : 0;
        if (SIXEL_FAILED(iface->vtbl->set_transparency(iface,
                &transparency))) {
            goto end;
        }
        mask = NULL;
        if (SIXEL_FAILED(sixel_encoder_encode_frame(encoder, frame, output))) {
            goto end;
        }
        dither = (sixel_dither_t *)encoder->diagnostic_dither;
        if (dither == NULL || dither->use_rgb101_palette != !byte ||
                !cm_painted(capture.data, painted, width, height) ||
                (masked && (!dither->keycolor_reserved ||
                 dither->keycolor != dither->ncolors - 1)) ||
                (!masked && dither->keycolor >= 0) ||
                dither->ncolors > 256 ||
                (boundary && dither->ncolors != 256) ||
                (!boundary && mode != 9 &&
                 (grow ? dither->ncolors <= 3 : dither->ncolors != 3))) {
            goto end;
        }
        if (!byte) {
            if (SIXEL_FAILED(dither->palette->vtbl->get_entries_float32(
                    dither->palette, &view)) || view.entries == NULL) {
                goto end;
            }
            /* Hidden magnitudes cannot enter representatives or soft cover.
             * Neither may the output key be counted among the real colors. */
            for (n = 0; n < (unsigned int)(dither->ncolors - masked) * 3;
                    ++n) {
                if (!isfinite(view.entries[n]) ||
                        view.entries[n] < 0.f || view.entries[n] > 1.5f) {
                    goto end;
                }
            }
        }
        for (i = 0; i < width * height; ++i) {
            hidden = masked && (i % width == width - 1 || mode == 9);
            if (hidden ? painted[i] != -1 : painted[i] < 0 ||
                    painted[i] >= dither->ncolors - masked) {
                goto end;
            }
        }
        if (hard && grow && !boundary && mode != 9 && strstr(capture.data,
                mode == 5 ? ";2;92;20;14" : ";2;100;0;0") == NULL) {
            goto end;
        }
        if (pass == 0) {
            first = capture;
        } else if ((mode == 10 || pass == 2) &&
                (first.size != capture.size ||
                 memcmp(first.data, capture.data, first.size) != 0)) {
            goto end;
        }
        sixel_frame_unref(frame);
        frame = NULL;
    }
    ok = 1;
end:
    if (!ok) {
        fprintf(stderr, "cover mask mode=%d: %s\n", mode,
            sixel_helper_get_additional_message());
    }
    free(mask);
    sixel_frame_unref(frame);
    sixel_output_unref(output);
    sixel_encoder_unref(encoder);
    return ok;
}

int
test_cover_mask(int argc, char **argv)
{
    int ok;

    ok = argc == 3 && strcmp(argv[1], "wire") == 0 ?
        cm_wire(atoi(argv[2])) : argc == 6 && strcmp(argv[1], "unit") == 0 ?
        cm_capacity((unsigned int)atoi(argv[2]),
            (unsigned int)atoi(argv[3]), atoi(argv[4]), atoi(argv[5])) : 0;
    if (!ok) {
        fprintf(stderr, "cover reservation contract failed\n");
    }
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
