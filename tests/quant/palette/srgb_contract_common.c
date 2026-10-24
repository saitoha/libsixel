/* SPDX-License-Identifier: MIT */
/* Literal wire values and independent sRGB equations protect the boundary. */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sixel.h>
#include "src/palette.h"
#include "src/lookup-policy-none.h"
#include "srgb_contract_common.h"

int
srgb_palette_contract(int linear)
{
    sixel_allocator_t *allocator;
    sixel_palette_t *palette;
    sixel_lookup_policy_interface_t *lookup;
    sixel_palette_entries_request_t br;
    sixel_palette_float32_entries_request_t fr;
    sixel_palette_entries_view_t bytes;
    sixel_palette_float32_entries_view_t floats;
    sixel_lookup_policy_prepare_request_t lr;
    unsigned char const *wire;
    unsigned int count;
    unsigned char input_bytes[6] = { 0, 0, 0, 0, 0, 0 };
    float input[6];
    float query[3];
    void *object;
    int code;
    int n;
    int ok;

    allocator = NULL;
    palette = NULL;
    lookup = NULL;
    object = NULL;
    ok = 0;
    memset(&lr, 0, sizeof(lr));
    for (n = 0; n < 6; ++n) {
        input[n] = linear ? (n < 3 ? .1f : .11f)
                          : (n < 3 ? .104f : .124f);
    }
    for (n = 0; n < 3; ++n) {
        query[n] = linear ? .106f : .112f;
    }
    if (SIXEL_FAILED(sixel_allocator_new(&allocator, NULL, NULL, NULL,
            NULL)) || SIXEL_FAILED(sixel_palette_factory_new(allocator,
            &object))) {
        goto end;
    }
    palette = object;
    br.entries = input_bytes;
    br.colors = 2;
    br.depth = 3;
    fr.entries = input;
    fr.colors = 2;
    fr.depth = 3 * sizeof(float);
    if (SIXEL_FAILED(palette->vtbl->init_entries(palette, &br)) ||
            SIXEL_FAILED(palette->vtbl->init_entries_float32(palette, &fr)) ||
            SIXEL_FAILED(sixel_palette_prepare_rgb101(palette,
                linear ? SIXEL_COLORSPACE_LINEAR : SIXEL_COLORSPACE_GAMMA,
                SIXEL_PALETTE_OUTPUT_LEGACY)) ||
            SIXEL_FAILED(sixel_palette_get_rgb101(palette, &wire, &count)) ||
            count != 2 ||
            SIXEL_FAILED(palette->vtbl->get_entries(palette, &bytes)) ||
            SIXEL_FAILED(palette->vtbl->get_entries_float32(palette,
                &floats))) {
        goto end;
    }
    for (n = 0; n < 6; ++n) {
        code = linear ? (n < 3 ? 35 : 37) : (n < 3 ? 10 : 12);
        if (wire[n] != code || floats.entries[n] != input[n] ||
                bytes.entries[n] != input_bytes[n]) {
            goto end;
        }
    }
    if (SIXEL_FAILED(sixel_lookup_policy_none_float32_new(allocator,
            &object))) {
        goto end;
    }
    lookup = object;
    lr.palette = bytes.entries;
    lr.palette_float = floats.entries;
    lr.depth = 3;
    lr.float_depth = 3 * sizeof(float);
    lr.reqcolor = 2;
    /* Quantizing only the wire cache must retain the original nearest
     * entry. Replacing the working palette with sent colors changes it. */
    ok = SIXEL_SUCCEEDED(lookup->vtbl->prepare(lookup, &lr)) &&
        lookup->vtbl->map_pixel(lookup, (unsigned char const *)query) ==
            (linear ? 1 : 0);
end:
    if (lookup != NULL) {
        lookup->vtbl->unref(lookup);
    }
    if (palette != NULL) {
        palette->vtbl->unref(palette);
    }
    sixel_allocator_unref(allocator);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}

typedef struct srgb_capture {
    char data[2048];
    size_t size;
} srgb_capture_t;

static int
srgb_contract_write(char *data, int size, void *context)
{
    srgb_capture_t *capture;

    capture = context;
    if (size < 0 || (size_t)size >= sizeof(capture->data) - capture->size) {
        return -1;
    }
    memcpy(capture->data + capture->size, data, (size_t)size);
    capture->size += (size_t)size;
    capture->data[capture->size] = '\0';
    return size;
}

int
srgb_encoder_contract(int linear)
{
    static struct {
        int flag;
        char const *value;
    } const options[] = {
        {'=', "balanced:B1"}, {'p', "1"}, {'.', "auto"}, {'U', "gamma"},
        {'Q', "kmeans:seed=1"}, {'X', "linear"}, {'F', "none"},
        {'a', "off"}, {'_', "none"}, {'~', "none"}, {'d', "fs"},
        {'t', "rgb"}, {'5', "none"}, {'4', "full-frame"}
    };
    sixel_encoder_t *encoder;
    sixel_output_t *output;
    sixel_frame_t *frame;
    srgb_capture_t capture;
    float pixels[6] = { 2.0f, .01f, .001f, -.5f, .01f, .001f };
    size_t n;
    int ok;

    encoder = NULL;
    output = NULL;
    frame = NULL;
    ok = 0;
    memset(&capture, 0, sizeof(capture));
    if (!linear) {
        /* Independently encoded sRGB values for linear (2, .01, .001)
         * and (-.5, .01, .001). This exercises a non-identity -X transform:
         * clipping before clustering would change the red mean to .5. */
        pixels[0] = 1.3532560461493863f;
        pixels[3] = -.7353569830524495f;
        pixels[1] = pixels[4] = .09985282273412832f;
        pixels[2] = pixels[5] = .01292f;
    }
    if (SIXEL_FAILED(sixel_encoder_new(&encoder, NULL)) ||
            SIXEL_FAILED(sixel_output_new(&output, srgb_contract_write,
                &capture, NULL))) {
        goto end;
    }
    for (n = 0; n < sizeof(options) / sizeof(options[0]); ++n) {
        if (SIXEL_FAILED(sixel_encoder_setopt(encoder, options[n].flag,
                options[n].value))) {
            goto end;
        }
    }
    if (SIXEL_FAILED(sixel_encoder_setopt(encoder, 'W',
            linear ? "linear" : "gamma")) ||
            SIXEL_FAILED(sixel_frame_new(&frame, NULL)) ||
            SIXEL_FAILED(sixel_frame_init_borrowed(frame, pixels, 2, 1,
                linear ? SIXEL_PIXELFORMAT_LINEARRGBFLOAT32
                       : SIXEL_PIXELFORMAT_RGBFLOAT32, NULL, -1))) {
        goto end;
    }
    /* Linear mean (.75, .01, .001) encodes to RGB101 (88, 10, 1).
     * Clipping either source before clustering changes the red centroid. */
    ok = SIXEL_SUCCEEDED(sixel_encoder_encode_frame(encoder, frame, output))
        && strstr(capture.data, "#0;2;88;10;1") != NULL;
end:
    if (!ok) {
        fprintf(stderr, "sRGB contract: %s: %s\n", capture.data,
                sixel_helper_get_additional_message());
    }
    sixel_frame_unref(frame);
    sixel_output_unref(output);
    sixel_encoder_unref(encoder);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
