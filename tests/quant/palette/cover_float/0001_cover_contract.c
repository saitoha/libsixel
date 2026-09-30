/* SPDX-License-Identifier: MIT */
/* Native cover: source support, independent RGB geometry and publication. */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sixel.h>
#include "src/encoder.h"
#include "src/dither.h"
#include "src/palette.h"
#include "src/palette-common-cover.h"
#include "tests/data/color-numeric/palette-space.h"

static int const cover_native_formats[] = {
    SIXEL_PIXELFORMAT_RGBFLOAT32, SIXEL_PIXELFORMAT_LINEARRGBFLOAT32,
    SIXEL_PIXELFORMAT_OKLABFLOAT32, SIXEL_PIXELFORMAT_CIELABFLOAT32,
    SIXEL_PIXELFORMAT_DIN99DFLOAT32
};
static char const *const cover_native_spaces[] = {
    "gamma", "linear", "oklab", "cielab", "din99d"
};
/* Independent neutral references: decode gamma 1/16, 1/8 and their gamma
 * midpoint 3/32, then apply the published D65 matrices/opponent equations.
 * Values use the Decimal oracle in generate_palette_space_fixtures.py and
 * are rounded once to binary32. Native-coordinate averaging is different. */
static double const cover_native_midpoints[5][3][3] = {
    { /* gamma */
        { 0.0625, 0.0625, 0.0625 },
        { 0.125, 0.125, 0.125 },
        { 0.09375, 0.09375, 0.09375 }
    },
    { /* linear */
        { 0.00515566859, 0.00515566859, 0.00515566859 },
        { 0.014349875, 0.014349875, 0.014349875 },
        { 0.00908010453, 0.00908010453, 0.00908010453 }
    },
    { /* oklab */
        { 0.172754079, 1.39849738e-11, 6.43921982e-09 },
        { 0.243005455, 1.96720383e-11, 9.05776343e-09 },
        { 0.208623677, 1.68887283e-11, 7.77621967e-09 }
    },
    { /* cielab */
        { 0.0465709679, -1.56825699e-08, 6.27302832e-09 },
        { 0.121886343, -3.16413349e-08, 1.26565345e-08 },
        { 0.0820034817, -2.71645408e-08, 1.08658167e-08 }
    },
    { /* din99d */
        { 0.0540728904, -3.00731067e-08, 1.33349598e-08 },
        { 0.139661387, -6.06758377e-08, 2.69047682e-08 },
        { 0.0946191549, -5.20910817e-08, 2.30981296e-08 }
    }
};

static int cover_native_fail;

static void *
cover_native_calloc(size_t count, size_t size)
{
    /* The candidate histogram is the only calloc with 4096 cells in this
     * fixture. Arm only for the repair stage; do not fail encoder creation. */
    if (cover_native_fail && count == 4096) {
        cover_native_fail = 0;
        return NULL;
    }
    return calloc(count, size);
}

static int
cover_native_midpoint(sixel_allocator_t *allocator, int space)
{
    float entries[SIXEL_PALETTE_MAX * 3];
    float samples[6];
    unsigned int count;
    unsigned int i;
    double expected;

    for (i = 0; i < 6; ++i) {
        entries[i] = (float)cover_native_midpoints[space][i / 3][i % 3];
        samples[i] = (float)color_palette_space[space].samples[0][i % 3];
    }
    count = 2;
    if (SIXEL_FAILED(sixel_palette_cover_float(entries, &count, samples,
            sizeof(samples), cover_native_formats[space],
            SIXEL_PALETTE_COVER_CORNERS, 0, allocator)) || count != 2) {
        return 0;
    }
    /* 2e-5 native units allow matrix/TRC inversion of rounded input near
     * black; a non-gamma native-space midpoint differs by over 6e-4. */
    for (i = 0; i < 6; ++i) {
        expected = i < 3 ? cover_native_midpoints[space][2][i]
            : color_palette_space[space].samples[0][i - 3];
        if (!isfinite(entries[i]) || fabs(entries[i] - expected) > 2e-5) {
            fprintf(stderr, "midpoint space=%d [%u]=%.9g expected %.9g\n",
                space, i, (double)entries[i], expected);
            return 0;
        }
    }
    return 1;
}

static int
cover_native_values(sixel_allocator_t *allocator, int mode, int space)
{
    float entries[SIXEL_PALETTE_MAX * 3];
    float original[SIXEL_PALETTE_MAX * 3];
    float samples[12];
    float input_copy[12];
    double expected[6] = { .1328125, 0, 0, -.25, 1.25, .5 };
    SIXELSTATUS status;
    unsigned int count;
    unsigned int old_count;
    unsigned int length;
    unsigned int i;
    unsigned int j;
    int policy;
    int grow;
    int typed;
    int matched;
    double tolerance;

    memset(entries, 0, sizeof(entries));
    count = 2;
    length = sizeof(samples);
    policy = SIXEL_PALETTE_COVER_CORNERS;
    grow = 0;
    typed = mode == 0 || mode == 1;
    for (i = 0; i < 12; ++i) {
        samples[i] = i % 3 == 0 ? -.25f : i % 3 == 1 ? 1.25f : .5f;
    }
    entries[0] = .125f;
    entries[3] = .140625f;
    if (typed) {
        memset(entries, 0, sizeof(entries));
        for (i = 0; i < 12; ++i) {
            samples[i] = (float)color_palette_space[space].samples[
                i / 6][i % 3];
        }
        grow = 1;
        if (mode == 1) {
            memcpy(entries, samples, 3 * sizeof(float));
            memcpy(entries + 3, samples + 6, 3 * sizeof(float));
        }
    } else if (mode == 3) {
        length = 3 * sizeof(float); /* A singleton cannot nominate an anchor. */
    } else if (mode == 4) {
        /* Adjacent source colors in one cell nominate their arithmetic mean,
         * not the cell center or an independently bounded cube corner. */
        samples[0] = samples[6] = -.28125f;
        expected[3] = -.265625;
    } else if (mode == 5) {
        /* A wide red address range must not shrink the red distance: the
         * candidate (-4,0,0) precedes (0,1,0), unlike unit-box geometry. */
        memset(entries, 0, sizeof(entries));
        memset(samples, 0, sizeof(samples));
        samples[0] = samples[3] = -4;
        samples[7] = samples[10] = 1;
        grow = 1;
    } else if (mode == 6) {
        count = SIXEL_PALETTE_MAX - 1;
        grow = 1;
    } else if (mode == 7) {
        cover_native_fail = 1;
    } else if (mode == 8) {
        samples[11] = NAN;
    } else if (mode == 9) {
        policy = SIXEL_PALETTE_COVER_OFF;
    } else if (mode == 10) {
        /* One far pair costs more than half the candidate gap. */
        entries[0] = -1;
        entries[3] = 1;
        memset(samples, 0, sizeof(samples));
        for (i = 0; i < 4; ++i) {
            samples[i * 3 + 1] = .25f;
        }
    } else if (mode == 11) {
        /* Three components at 24/255 are on the reach threshold. Use 1/16
         * and 1/8 around it, avoiding floating-point boundary ambiguity. */
        memset(entries, 0, sizeof(entries));
        for (i = 0; i < 12; ++i) {
            samples[i] = .0625f;
        }
        grow = 1;
    }
    if (mode == 12 || mode == 13) {
        count = mode == 12 ? 31 : 32;
        policy = SIXEL_PALETTE_COVER_AUTO;
        grow = 1;
    }
    old_count = count;
    memcpy(original, entries, sizeof(entries));
    memcpy(input_copy, samples, sizeof(samples));
    status = sixel_palette_cover_float(entries, &count, samples, length,
        cover_native_formats[space], policy, grow, allocator);
    if (memcmp(samples, input_copy, sizeof(samples)) != 0) {
        return 0;
    }
    if (mode == 7 || mode == 8) {
        if (status != (mode == 7 ? SIXEL_BAD_ALLOCATION : SIXEL_BAD_INPUT) ||
                cover_native_fail || count != old_count ||
                memcmp(entries, original, sizeof(entries)) != 0) {
            return 0;
        }
        /* A failed selected repair must not poison the next operation. */
        samples[11] = .5f;
        status = sixel_palette_cover_float(entries, &count, samples, length,
            cover_native_formats[space], policy, grow, allocator);
    }
    if (SIXEL_FAILED(status)) {
        return 0;
    }
    if (mode == 1 || mode == 3 || mode == 9 || mode == 10 || mode == 11 ||
            mode == 12) {
        return count == old_count &&
            memcmp(entries, original, sizeof(entries)) == 0;
    }
    if (mode == 0) {
        if (count != 4 || memcmp(entries, original, 6 * sizeof(float))) {
            return 0;
        }
        /* Independently generated physical colors, rounded to input float32.
         * Allow 2e-5 native units for two matrix/TRC passes and cancellation
         * in opponent coordinates; clipping changes these by >0.1. */
        tolerance = 2e-5;
        for (i = 2; i < 4; ++i) {
            matched = 0;
            for (j = 0; j < 2; ++j) {
                if (isfinite(entries[i * 3]) &&
                        isfinite(entries[i * 3 + 1]) &&
                        isfinite(entries[i * 3 + 2]) &&
                        fabs(entries[i * 3] - samples[j * 6]) < tolerance &&
                        fabs(entries[i * 3 + 1] - samples[j * 6 + 1]) <
                            tolerance &&
                        fabs(entries[i * 3 + 2] - samples[j * 6 + 2]) <
                            tolerance) {
                    matched = 1;
                }
            }
            if (!matched) {
                return 0;
            }
        }
        return fabs(entries[6] - entries[9]) > .1;
    }
    if (mode == 5) {
        return count == 4 && entries[6] == -4 && entries[7] == 0 &&
            entries[8] == 0 && entries[9] == 0 && entries[10] == 1 &&
            entries[11] == 0;
    }
    if (mode == 6 || mode == 13) {
        return count == old_count + 1 &&
            memcmp(entries, original, old_count * 3 * sizeof(float)) == 0 &&
            entries[old_count * 3] == -.25f &&
            entries[old_count * 3 + 1] == 1.25f &&
            entries[old_count * 3 + 2] == .5f;
    }
    if (count != 2) {
        return 0;
    }
    /* Binary fractions and gamma identity make these exact value contracts. */
    for (i = 0; i < 6; ++i) {
        if (entries[i] != expected[i]) {
            fprintf(stderr, "entry[%u]=%.9g expected %.9g\n",
                i, (double)entries[i], expected[i]);
            return 0;
        }
    }
    return 1;
}

typedef struct cover_native_capture {
    char data[4096];
    size_t size;
} cover_native_capture_t;

static int
cover_native_write(char *data, int size, void *context)
{
    cover_native_capture_t *capture;

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
cover_native_wire(sixel_allocator_t *allocator, int model, int space, int mode)
{
    static struct { int flag; char const *value; } const options[] = {
        { '=', "1" }, { 'p', "2" }, { 'F', "none" },
        { '_', "none" }, { '~', "none" }, { 'd', "none" },
        { 't', "rgb" }, { '4', "full-frame" }, { '5', "none" }
    };
    static char const *const models[] = {
        "kmeans:seed=1", "center:seed=1", "medoids:seed=1", "heckbert", "auto"
    };
    sixel_encoder_t *encoder;
    sixel_output_t *output;
    sixel_frame_t *frame;
    sixel_dither_t *diagnostic;
    sixel_palette_float32_entries_view_t view;
    cover_native_capture_t capture;
    float pixels[18];
    unsigned char bytes[18];
    unsigned int n;
    int ok;
    int byte;
    SIXELSTATUS status;

    encoder = NULL;
    output = NULL;
    frame = NULL;
    ok = 0;
    byte = mode == 2;
    memset(&capture, 0, sizeof(capture));
    for (n = 0; n < 18; ++n) {
        pixels[n] = n < 6 ? 0 : n < 12 ? .5f : 1.5f;
        bytes[n] = n < 6 ? 0 : n < 12 ? 128 : 255;
    }
    if (SIXEL_FAILED(sixel_encoder_new(&encoder, allocator)) ||
            SIXEL_FAILED(sixel_output_new(&output, cover_native_write,
                &capture, allocator))) {
        goto end;
    }
    for (n = 0; n < sizeof(options) / sizeof(options[0]); ++n) {
        if (SIXEL_FAILED(sixel_encoder_setopt(encoder, options[n].flag,
                options[n].value))) {
            goto end;
        }
    }
    if (SIXEL_FAILED(sixel_encoder_setopt(encoder, 'Q', models[model])) ||
            SIXEL_FAILED(sixel_encoder_setopt(encoder, '.',
                byte ? "8bit" : "auto")) ||
            SIXEL_FAILED(sixel_encoder_setopt(encoder, 'a',
                mode == 3 ? "corners:Whard" :
                mode == 5 || mode == 6 ? "corners:V1" :
                "corners:Wsoft:V1")) ||
            SIXEL_FAILED(sixel_encoder_setopt(encoder, 'X',
                cover_native_spaces[space])) ||
            SIXEL_FAILED(sixel_encoder_setopt(encoder, 'W',
                cover_native_spaces[space])) ||
            SIXEL_FAILED(sixel_encoder_setopt(encoder, 'U',
                mode == 1 || mode == 3 || mode == 5
                ? "display-p3" : "gamma")) ||
            SIXEL_FAILED(sixel_frame_new(&frame, allocator)) ||
            SIXEL_FAILED(sixel_frame_init_borrowed(frame,
                byte ? (void *)bytes : (void *)pixels, 3, 2,
                byte ? SIXEL_PIXELFORMAT_RGB888 :
                SIXEL_PIXELFORMAT_RGBFLOAT32, NULL, -1))) {
        goto end;
    }
    if (mode == 7) {
        if (SIXEL_FAILED(sixel_encoder_setopt(encoder, '=', "4")) ||
                SIXEL_FAILED(sixel_encoder_setopt(encoder, '4',
                    "adaptive-grid"))) {
            goto end;
        }
    }
    cover_native_fail = mode == 4;
    status = sixel_encoder_encode_frame(encoder, frame, output);
    if (mode == 3 || mode == 5) {
        ok = status == SIXEL_BAD_ARGUMENT && capture.size == 0;
        goto end;
    }
    if (mode == 4) {
        if (status != SIXEL_BAD_ALLOCATION || cover_native_fail ||
                capture.size != 0) {
            goto end;
        }
        status = sixel_encoder_encode_frame(encoder, frame, output);
    }
    diagnostic = (sixel_dither_t *)encoder->diagnostic_dither;
    if (SIXEL_FAILED(status) || diagnostic == NULL) {
        goto end;
    }
    if (byte) {
        ok = diagnostic->use_rgb101_palette == 0 &&
            diagnostic->pixelformat == SIXEL_PIXELFORMAT_RGB888;
        goto end;
    }
    if (diagnostic->use_rgb101_palette == 0 ||
            diagnostic->pixelformat != cover_native_formats[space] ||
            SIXEL_FAILED(diagnostic->palette->vtbl->get_entries_float32(
                diagnostic->palette, &view)) || view.entries == NULL ||
            view.entry_count <= 2 || view.entry_count > 5) {
        goto end;
    }
    /* Grow must have reached the post-solver consumer. This fixture's three
     * source colors cannot all lie within the reach threshold of two entries.
     * The source value 1.5 also distinguishes a byte-clipped retry. */
    ok = strstr(capture.data, ";2;100;100;100") != NULL;
    if (space == 0) {
        for (n = 0; n < view.entry_count; ++n) {
            if (view.entries[n * 3] == 1.5f) {
                break;
            }
        }
        ok = ok && n < view.entry_count;
    }
end:
    if (!ok) {
        fprintf(stderr, "wire model=%d space=%d mode=%d: %s; %s\n",
            model, space, mode, sixel_helper_get_additional_message(),
            capture.data);
    }
    sixel_frame_unref(frame);
    sixel_output_unref(output);
    sixel_encoder_unref(encoder);
    return ok;
}

int
test_palette_cover_native(int argc, char **argv)
{
    sixel_allocator_t *allocator;
    int ok;

    if (argc != 5) {
        return EXIT_FAILURE;
    }
    allocator = NULL;
    if (SIXEL_FAILED(sixel_allocator_new(&allocator, NULL,
            cover_native_calloc, NULL, NULL))) {
        return EXIT_FAILURE;
    }
    ok = strcmp(argv[1], "wire") == 0
        ? cover_native_wire(allocator, atoi(argv[2]), atoi(argv[3]),
            atoi(argv[4]))
        : strcmp(argv[1], "midpoint") == 0
        ? cover_native_midpoint(allocator, atoi(argv[3]))
        : cover_native_values(allocator, atoi(argv[2]), atoi(argv[3]));
    sixel_allocator_unref(allocator);
    if (!ok) {
        fprintf(stderr, "cover %s %s %s %s: %s\n", argv[1], argv[2],
            argv[3], argv[4], sixel_helper_get_additional_message());
    }
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
