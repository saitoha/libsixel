/* SPDX-License-Identifier: MIT */
/* Independent typed centroids, immutable working palettes and wire values. */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sixel.h>
#include "src/palette.h"
#include "src/lookup-policy-none.h"
#include "tests/data/color-numeric/palette-space.h"

static char const *const space_names[] = {
    "gamma", "linear", "oklab", "cielab", "din99d"
};
static int const space_colors[] = {
    SIXEL_COLORSPACE_GAMMA, SIXEL_COLORSPACE_LINEAR, SIXEL_COLORSPACE_OKLAB,
    SIXEL_COLORSPACE_CIELAB, SIXEL_COLORSPACE_DIN99D
};
static int const space_formats[] = {
    SIXEL_PIXELFORMAT_RGBFLOAT32, SIXEL_PIXELFORMAT_LINEARRGBFLOAT32,
    SIXEL_PIXELFORMAT_OKLABFLOAT32, SIXEL_PIXELFORMAT_CIELABFLOAT32,
    SIXEL_PIXELFORMAT_DIN99DFLOAT32
};

typedef struct space_capture {
    char bytes[2048];
    size_t size;
} space_capture_t;

static int
space_write(char *data, int size, void *context)
{
    space_capture_t *capture;

    capture = context;
    if (size < 0 || (size_t)size >= sizeof(capture->bytes) - capture->size) {
        return -1;
    }
    memcpy(capture->bytes + capture->size, data, (size_t)size);
    capture->size += (size_t)size;
    capture->bytes[capture->size] = '\0';
    return size;
}

static int
space_wire(int x, int w, int p3, char const *binning, char const *snap)
{
    static struct {
        int flag;
        char const *value;
    } const options[] = {
        { '=', "balanced:B1" }, { 'p', "1" }, { '.', "float32" },
        { 'Q', "kmeans:seed=1" }, { 'F', "none" }, { 'a', "off" },
        { '~', "none" }, { 'd', "none" },
        { 't', "rgb" }, { '4', "full-frame" }
    };
    sixel_encoder_t *encoder;
    sixel_output_t *output;
    sixel_frame_t *frame;
    space_capture_t capture;
    float pixels[6] = { 2.0f, .1f, .1f, -.25f, .1f, .1f };
    unsigned char const *wire;
    char expected[64];
    size_t n;
    int ok;

    encoder = NULL;
    output = NULL;
    frame = NULL;
    ok = 0;
    memset(&capture, 0, sizeof(capture));
    wire = color_palette_space[x].wire[p3];
    snprintf(expected, sizeof(expected), "#0;2;%u;%u;%u#",
        (unsigned)wire[0], (unsigned)wire[1], (unsigned)wire[2]);
    if (SIXEL_FAILED(sixel_encoder_new(&encoder, NULL)) ||
            SIXEL_FAILED(sixel_output_new(&output, space_write,
                &capture, NULL))) {
        goto end;
    }
    for (n = 0U; n < sizeof(options) / sizeof(options[0]); ++n) {
        if (SIXEL_FAILED(sixel_encoder_setopt(encoder, options[n].flag,
                options[n].value))) {
            goto end;
        }
    }
    if (SIXEL_FAILED(sixel_encoder_setopt(encoder, '_', snap)) ||
            SIXEL_FAILED(sixel_encoder_setopt(encoder, '5', binning)) ||
            SIXEL_FAILED(sixel_encoder_setopt(encoder, 'U',
                p3 ? "display-p3" : "gamma")) ||
            SIXEL_FAILED(sixel_encoder_setopt(encoder, 'X', space_names[x])) ||
            SIXEL_FAILED(sixel_encoder_setopt(encoder, 'W', space_names[w])) ||
            SIXEL_FAILED(sixel_frame_new(&frame, NULL)) ||
            SIXEL_FAILED(sixel_frame_init_borrowed(frame, pixels, 2, 1,
                SIXEL_PIXELFORMAT_LINEARRGBFLOAT32, NULL, -1))) {
        goto end;
    }
    /* One cluster fixes the centroid in X. W may change coordinates but
     * not the output color. Independent matrices/TRCs fix the wire code,
     * with ample rounding margin after binary32 intermediate stores. */
    ok = SIXEL_SUCCEEDED(sixel_encoder_encode_frame(encoder, frame, output))
        && strstr(capture.bytes, expected) != NULL;
end:
    if (!ok) {
        fprintf(stderr, "wire X=%s W=%s p3=%d: expected %s; got %s; %s\n",
            space_names[x], space_names[w], p3, expected, capture.bytes,
            sixel_helper_get_additional_message());
    }
    sixel_frame_unref(frame);
    sixel_output_unref(output);
    sixel_encoder_unref(encoder);
    return ok;
}

static int
space_centroid(sixel_palette_t *palette, int space)
{
    sixel_palette_generate_request_t request;
    sixel_palette_build_attempt_t attempt;
    sixel_palette_float32_entries_view_t view;
    float samples[6];
    float extended[6] = { 1.25f, .25f, -.375f, 1.125f, .3125f, -.125f };
    double fixed[3] = { 1.1875, .28125, -.25 };
    double expected;
    SIXELSTATUS status;
    SIXELSTATUS finish;
    int trial;
    int n;

    memset(&request, 0, sizeof(request));
    request.length = sizeof(samples);
    request.pixelformat = space_formats[space];
    request.requested_colors = 1;
    request.prefer_float32 = 1;
    request.quantize_model = SIXEL_QUANTIZE_MODEL_KMEANS;
    request.final_merge_mode = SIXEL_FINAL_MERGE_NONE;
    request.lut_policy = SIXEL_LUT_POLICY_NONE;
    for (n = 0; n < 6; ++n) {
        samples[n] = (float)color_palette_space[space].samples[n / 3][n % 3];
    }
    /* A physical extended RGB pair and a native-coordinate excursion both
     * retain their means. The second case also detects Lab-axis clipping
     * even when the physical pair happens to lie inside nominal bounds. */
    for (trial = 0; trial < 2; ++trial) {
        request.data = trial == 0 ? samples : extended;
        memset(&attempt, 0, sizeof(attempt));
        sixel_palette_binning_state_init(&attempt.binning,
            SIXEL_PALETTE_BINNING_NONE, SIXEL_PALETTE_POLICY_ORIGIN_EXPLICIT);
        if (SIXEL_FAILED(sixel_palette_build_attempt_begin(palette,
                &attempt))) {
            return 0;
        }
        status = sixel_palette_generate_extended(palette, &request);
        finish = sixel_palette_build_attempt_finish(palette, &attempt);
        if (SIXEL_FAILED(status) || SIXEL_FAILED(finish) ||
                SIXEL_FAILED(palette->vtbl->get_entries_float32(palette,
                    &view)) || view.entries == NULL || view.entry_count != 1) {
            return 0;
        }
        for (n = 0; n < 3; ++n) {
            expected = trial == 0 ? color_palette_space[space].center[n]
                                  : fixed[n];
            /* The independent oracle explicitly rounds to binary32. This
             * tolerance allows a few storage ULPs, not nominal clipping. */
            if (!isfinite(view.entries[n]) ||
                    fabs(view.entries[n] - expected) > 5e-7) {
                fprintf(stderr, "centroid %s[%d]: %.9g != %.9g\n",
                    space_names[space], n, (double)view.entries[n], expected);
                return 0;
            }
        }
    }
    return 1;
}

static int
space_lookup(sixel_palette_t *palette, sixel_allocator_t *allocator,
             int space, int p3)
{
    sixel_palette_entries_request_t br;
    sixel_palette_float32_entries_request_t fr;
    sixel_palette_entries_view_t bytes;
    sixel_palette_float32_entries_view_t floats;
    sixel_lookup_policy_prepare_request_t prepare;
    sixel_lookup_policy_interface_t *lookup;
    unsigned char byte_values[6] = { 7, 13, 29, 31, 43, 59 };
    unsigned char const *wire;
    unsigned int count;
    float values[6];
    float query[3];
    void *object;
    int n;
    int ok;

    lookup = NULL;
    object = NULL;
    ok = 0;
    memset(&prepare, 0, sizeof(prepare));
    for (n = 0; n < 6; ++n) {
        values[n] = (float)color_palette_space[space].lookup[n / 3][n % 3];
    }
    for (n = 0; n < 3; ++n) {
        query[n] = .25f * values[n] + .75f * values[n + 3];
    }
    br.entries = byte_values;
    br.colors = 2;
    br.depth = 3;
    fr.entries = values;
    fr.colors = 2;
    fr.depth = 3 * sizeof(float);
    if (SIXEL_FAILED(palette->vtbl->init_entries(palette, &br)) ||
            SIXEL_FAILED(palette->vtbl->init_entries_float32(palette, &fr)) ||
            SIXEL_FAILED(sixel_lookup_policy_none_float32_new(allocator,
                &object))) {
        goto end;
    }
    lookup = object;
    if (SIXEL_FAILED(sixel_palette_prepare_rgb101(palette,
            space_colors[space], p3 ? SIXEL_PALETTE_OUTPUT_DISPLAY_P3
                                    : SIXEL_PALETTE_OUTPUT_LEGACY)) ||
            SIXEL_FAILED(sixel_palette_get_rgb101(palette, &wire, &count)) ||
            count != 2 || memcmp(wire,
                color_palette_space[space].lookup_wire[p3], 6) != 0 ||
            SIXEL_FAILED(palette->vtbl->get_entries(palette, &bytes)) ||
            SIXEL_FAILED(palette->vtbl->get_entries_float32(palette,
                &floats)) ||
            memcmp(bytes.entries, byte_values, sizeof(byte_values)) != 0 ||
            memcmp(floats.entries, values, sizeof(values)) != 0) {
        goto end;
    }
    prepare.palette = bytes.entries;
    prepare.palette_float = floats.entries;
    prepare.depth = 3;
    prepare.float_depth = 3 * sizeof(float);
    prepare.reqcolor = 2;
    ok = SIXEL_SUCCEEDED(lookup->vtbl->prepare(lookup, &prepare)) &&
        lookup->vtbl->map_pixel(lookup, (unsigned char const *)query) == 1;
end:
    if (lookup != NULL) {
        lookup->vtbl->unref(lookup);
    }
    return ok;
}

int
test_palette_space(int argc, char **argv)
{
    sixel_allocator_t *allocator;
    sixel_palette_t *palette;
    void *object;
    int x;
    int w;
    int p3;
    int ok;

    if (argc < 5 || argc > 7) {
        return EXIT_FAILURE;
    }
    x = atoi(argv[2]);
    w = atoi(argv[3]);
    p3 = atoi(argv[4]);
    if (x < 0 || x >= 5 || w < 0 || w >= 5 || p3 < 0 || p3 > 1) {
        return EXIT_FAILURE;
    }
    if (strcmp(argv[1], "wire") == 0) {
        return space_wire(x, w, p3, argc >= 6 ? argv[5] : "none",
            argc == 7 ? argv[6] : "none")
            ? EXIT_SUCCESS : EXIT_FAILURE;
    }
    allocator = NULL;
    palette = NULL;
    object = NULL;
    ok = 0;
    if (SIXEL_SUCCEEDED(sixel_allocator_new(&allocator, NULL, NULL,
            NULL, NULL)) &&
            SIXEL_SUCCEEDED(sixel_palette_factory_new(allocator, &object))) {
        palette = object;
        ok = strcmp(argv[1], "centroid") == 0
            ? space_centroid(palette, x)
            : space_lookup(palette, allocator, x, p3);
    }
    if (!ok) {
        fprintf(stderr, "%s %s p3=%d: %s\n", argv[1], space_names[x], p3,
            sixel_helper_get_additional_message());
    }
    if (palette != NULL) {
        palette->vtbl->unref(palette);
    }
    sixel_allocator_unref(allocator);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
