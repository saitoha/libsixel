/*
 * SPDX-License-Identifier: MIT
 *
 * Verify encoder-side accumulation buffers produce transparent delta output.
 */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <6cells.h>
#include <sixel.h>

#define ACCUMULATION_WIDTH 18
#define ACCUMULATION_HEIGHT 18
#define ACCUMULATION_PIXELS (ACCUMULATION_WIDTH * ACCUMULATION_HEIGHT)
#define ACCUMULATION_BYTES (ACCUMULATION_PIXELS * 3)
#define ACCUMULATION_RGBA_BYTES (ACCUMULATION_PIXELS * 4)
#define ACCUMULATION_OUTPUT_CAPACITY 32768

typedef struct accumulation_payload {
    unsigned char bytes[ACCUMULATION_OUTPUT_CAPACITY];
    int size;
} accumulation_payload_t;

static int
accumulation_write(char *data, int size, void *priv)
{
    accumulation_payload_t *payload;
    int room;

    payload = (accumulation_payload_t *)priv;
    if (data == NULL || payload == NULL || size < 0) {
        return 1;
    }
    room = ACCUMULATION_OUTPUT_CAPACITY - payload->size;
    if (size > room) {
        return 1;
    }
    memcpy(payload->bytes + payload->size, data, (size_t)size);
    payload->size += size;

    return 0;
}

static int
accumulation_contains(unsigned char const *data,
                      int size,
                      char const *needle)
{
    int needle_size;
    int index;

    needle_size = 0;
    index = 0;
    if (data == NULL || size <= 0 || needle == NULL) {
        return 0;
    }
    needle_size = (int)strlen(needle);
    if (needle_size <= 0 || size < needle_size) {
        return 0;
    }
    for (index = 0; index <= size - needle_size; ++index) {
        if (memcmp(data + index, needle, (size_t)needle_size) == 0) {
            return 1;
        }
    }

    return 0;
}

static SIXELSTATUS
accumulation_make_frame(sixel_allocator_t *allocator,
                        unsigned char const *source,
                        sixel_frame_t **frame_out)
{
    SIXELSTATUS status;
    sixel_frame_t *frame;
    unsigned char *pixels;

    status = SIXEL_FALSE;
    frame = NULL;
    pixels = NULL;
    if (allocator == NULL || source == NULL || frame_out == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    *frame_out = NULL;

    status = sixel_frame_new(&frame, allocator);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    pixels = (unsigned char *)sixel_allocator_malloc(allocator,
                                                     ACCUMULATION_BYTES);
    if (pixels == NULL) {
        status = SIXEL_BAD_ALLOCATION;
        goto end;
    }
    memcpy(pixels, source, ACCUMULATION_BYTES);

    status = sixel_frame_init(frame,
                              pixels,
                              ACCUMULATION_WIDTH,
                              ACCUMULATION_HEIGHT,
                              SIXEL_PIXELFORMAT_RGB888,
                              NULL,
                              (-1));
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    pixels = NULL;
    sixel_frame_set_colorspace(frame, SIXEL_COLORSPACE_GAMMA);
    *frame_out = frame;
    frame = NULL;
    status = SIXEL_OK;

end:
    sixel_allocator_free(allocator, pixels);
    sixel_frame_unref(frame);
    return status;
}

static SIXELSTATUS
accumulation_make_rgba_frame(sixel_allocator_t *allocator,
                             unsigned char const *source,
                             sixel_frame_t **frame_out)
{
    SIXELSTATUS status;
    sixel_frame_t *frame;
    sixel_frame_interface_t *frame_if;
    sixel_frame_transparency_t transparency;
    unsigned char *pixels;

    status = SIXEL_FALSE;
    frame = NULL;
    frame_if = NULL;
    memset(&transparency, 0, sizeof(transparency));
    pixels = NULL;
    if (allocator == NULL || source == NULL || frame_out == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    *frame_out = NULL;

    status = sixel_frame_new(&frame, allocator);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    pixels = (unsigned char *)sixel_allocator_malloc(
        allocator,
        ACCUMULATION_RGBA_BYTES);
    if (pixels == NULL) {
        status = SIXEL_BAD_ALLOCATION;
        goto end;
    }
    memcpy(pixels, source, ACCUMULATION_RGBA_BYTES);

    status = sixel_frame_init(frame,
                              pixels,
                              ACCUMULATION_WIDTH,
                              ACCUMULATION_HEIGHT,
                              SIXEL_PIXELFORMAT_RGBA8888,
                              NULL,
                              (-1));
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    frame_if = sixel_frame_as_interface(frame);
    if (frame_if == NULL || frame_if->vtbl == NULL ||
        frame_if->vtbl->set_transparency == NULL) {
        status = SIXEL_BAD_ARGUMENT;
        goto end;
    }
    transparency.transparent = (-1);
    transparency.alpha_zero_is_transparent = 1;
    transparency.transparent_mask = NULL;
    transparency.transparent_mask_size = 0u;
    status = frame_if->vtbl->set_transparency(frame_if, &transparency);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    pixels = NULL;
    sixel_frame_set_colorspace(frame, SIXEL_COLORSPACE_GAMMA);
    *frame_out = frame;
    frame = NULL;
    status = SIXEL_OK;

end:
    sixel_allocator_free(allocator, pixels);
    sixel_frame_unref(frame);
    return status;
}

static SIXELSTATUS
accumulation_encode(sixel_allocator_t *allocator,
                    unsigned char const *previous,
                    unsigned char const *current,
                    char const *sixdelta_threshold,
                    int *size_out,
                    int *has_keep_header_out)
{
    SIXELSTATUS status;
    sixel_encoder_t *encoder;
    sixel_frame_t *frame;
    sixel_output_t *output;
    accumulation_payload_t payload;

    status = SIXEL_FALSE;
    encoder = NULL;
    frame = NULL;
    output = NULL;
    memset(&payload, 0, sizeof(payload));
    if (allocator == NULL || current == NULL || size_out == NULL ||
        has_keep_header_out == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    *size_out = 0;
    *has_keep_header_out = 0;

    status = sixel_encoder_new(&encoder, allocator);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    status = sixel_encoder_setopt(encoder, SIXEL_OPTFLAG_COLORS, "4");
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    status = sixel_encoder_setopt(encoder, SIXEL_OPTFLAG_DIFFUSION, "none");
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    status = sixel_encoder_setopt(encoder,
                                  SIXEL_OPTFLAG_TRANSPARENT_POLICY,
                                  "keep");
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    if (sixdelta_threshold != NULL) {
        status = sixel_encoder_setopt(encoder,
                                      SIXEL_OPTFLAG_6DELTA_THRESHOLD,
                                      sixdelta_threshold);
        if (SIXEL_FAILED(status)) {
            goto end;
        }
    }
    if (previous != NULL) {
        status = sixel_encoder_set_accumulation_buffer(
            encoder,
            previous,
            ACCUMULATION_WIDTH,
            ACCUMULATION_HEIGHT,
            SIXEL_PIXELFORMAT_RGB888);
        if (SIXEL_FAILED(status)) {
            goto end;
        }
    }

    status = accumulation_make_frame(allocator, current, &frame);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    status = sixel_output_new(&output,
                              accumulation_write,
                              &payload,
                              allocator);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    status = sixel_encoder_encode_frame(encoder, frame, output);
    if (SIXEL_FAILED(status)) {
        goto end;
    }

    *size_out = payload.size;
    *has_keep_header_out = accumulation_contains(payload.bytes,
                                                 payload.size,
                                                 "\033P0;1q");
    status = SIXEL_OK;

end:
    if (output != NULL) {
        sixel_output_unref(output);
    }
    sixel_frame_unref(frame);
    if (encoder != NULL) {
        sixel_encoder_unref(encoder);
    }
    return status;
}

static SIXELSTATUS
accumulation_encode_sequence(sixel_allocator_t *allocator,
                             unsigned char const *first,
                             unsigned char const *second,
                             int *first_size_out,
                             int *second_size_out,
                             int *second_has_keep_header_out)
{
    SIXELSTATUS status;
    sixel_encoder_t *encoder;
    sixel_frame_t *frame;
    sixel_output_t *output;
    accumulation_payload_t payload;

    status = SIXEL_FALSE;
    encoder = NULL;
    frame = NULL;
    output = NULL;
    memset(&payload, 0, sizeof(payload));
    if (allocator == NULL || first == NULL || second == NULL ||
        first_size_out == NULL || second_size_out == NULL ||
        second_has_keep_header_out == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    *first_size_out = 0;
    *second_size_out = 0;
    *second_has_keep_header_out = 0;

    status = sixel_encoder_new(&encoder, allocator);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    status = sixel_encoder_setopt(encoder, SIXEL_OPTFLAG_COLORS, "4");
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    status = sixel_encoder_setopt(encoder, SIXEL_OPTFLAG_DIFFUSION, "none");
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    status = sixel_encoder_setopt(encoder,
                                  SIXEL_OPTFLAG_TRANSPARENT_POLICY,
                                  "keep");
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    /*
     * Auto-seeded accumulation stores quantized display RGB, not the source
     * frame.  Permit one LSB so stable source colors can still exercise the
     * retained-plane path after palette application.
     */
    status = sixel_encoder_setopt(encoder,
                                  SIXEL_OPTFLAG_6DELTA_THRESHOLD,
                                  "8");
    if (SIXEL_FAILED(status)) {
        goto end;
    }

    status = accumulation_make_frame(allocator, first, &frame);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    status = sixel_output_new(&output,
                              accumulation_write,
                              &payload,
                              allocator);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    status = sixel_encoder_encode_frame(encoder, frame, output);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    *first_size_out = payload.size;
    sixel_output_unref(output);
    output = NULL;
    sixel_frame_unref(frame);
    frame = NULL;
    memset(&payload, 0, sizeof(payload));

    status = accumulation_make_frame(allocator, second, &frame);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    status = sixel_output_new(&output,
                              accumulation_write,
                              &payload,
                              allocator);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    status = sixel_encoder_encode_frame(encoder, frame, output);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    *second_size_out = payload.size;
    *second_has_keep_header_out = accumulation_contains(payload.bytes,
                                                        payload.size,
                                                        "\033P0;1q");
    status = SIXEL_OK;

end:
    if (output != NULL) {
        sixel_output_unref(output);
    }
    sixel_frame_unref(frame);
    if (encoder != NULL) {
        sixel_encoder_unref(encoder);
    }
    return status;
}

static SIXELSTATUS
accumulation_encode_sequence_delta3(sixel_allocator_t *allocator,
                                    unsigned char const *first,
                                    unsigned char const *second,
                                    unsigned char const *third,
                                    char const *sixdelta_threshold,
                                    int *first_size_out,
                                    int *second_size_out,
                                    int *third_size_out)
{
    SIXELSTATUS status;
    sixel_encoder_t *encoder;
    sixel_frame_t *frame;
    sixel_output_t *output;
    accumulation_payload_t payload;
    unsigned char const *frames[3];
    int *sizes[3];
    int frame_index;

    status = SIXEL_FALSE;
    encoder = NULL;
    frame = NULL;
    output = NULL;
    memset(&payload, 0, sizeof(payload));
    frames[0] = first;
    frames[1] = second;
    frames[2] = third;
    sizes[0] = first_size_out;
    sizes[1] = second_size_out;
    sizes[2] = third_size_out;
    frame_index = 0;
    if (allocator == NULL || first == NULL || second == NULL ||
        third == NULL || sixdelta_threshold == NULL ||
        first_size_out == NULL || second_size_out == NULL ||
        third_size_out == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    *first_size_out = 0;
    *second_size_out = 0;
    *third_size_out = 0;

    status = sixel_encoder_new(&encoder, allocator);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    status = sixel_encoder_setopt(encoder, SIXEL_OPTFLAG_COLORS, "4");
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    status = sixel_encoder_setopt(encoder, SIXEL_OPTFLAG_DIFFUSION, "none");
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    status = sixel_encoder_setopt(encoder,
                                  SIXEL_OPTFLAG_TRANSPARENT_POLICY,
                                  "keep");
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    status = sixel_encoder_setopt(encoder,
                                  SIXEL_OPTFLAG_6DELTA_THRESHOLD,
                                  sixdelta_threshold);
    if (SIXEL_FAILED(status)) {
        goto end;
    }

    for (frame_index = 0; frame_index < 3; ++frame_index) {
        status = accumulation_make_frame(allocator,
                                         frames[frame_index],
                                         &frame);
        if (SIXEL_FAILED(status)) {
            goto end;
        }
        status = sixel_output_new(&output,
                                  accumulation_write,
                                  &payload,
                                  allocator);
        if (SIXEL_FAILED(status)) {
            goto end;
        }
        status = sixel_encoder_encode_frame(encoder, frame, output);
        if (SIXEL_FAILED(status)) {
            goto end;
        }
        *sizes[frame_index] = payload.size;
        sixel_output_unref(output);
        output = NULL;
        sixel_frame_unref(frame);
        frame = NULL;
        memset(&payload, 0, sizeof(payload));
    }
    status = SIXEL_OK;

end:
    if (output != NULL) {
        sixel_output_unref(output);
    }
    sixel_frame_unref(frame);
    if (encoder != NULL) {
        sixel_encoder_unref(encoder);
    }
    return status;
}

static SIXELSTATUS
accumulation_hidden_alpha_seed_second_alpha(sixel_allocator_t *allocator,
                                           unsigned char const *first,
                                           unsigned char const *second,
                                           unsigned char *alpha_out)
{
    SIXELSTATUS status;
    sixel_encoder_t *encoder;
    sixel_frame_t *frame;
    sixel_output_t *output;
    sixel_decode_options_t options;
    sixel_decode_result_t result;
    accumulation_payload_t payload;

    status = SIXEL_FALSE;
    encoder = NULL;
    frame = NULL;
    output = NULL;
    memset(&options, 0, sizeof(options));
    memset(&result, 0, sizeof(result));
    memset(&payload, 0, sizeof(payload));
    if (allocator == NULL || first == NULL || second == NULL ||
        alpha_out == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    *alpha_out = 0u;

    status = sixel_encoder_new(&encoder, allocator);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    status = sixel_encoder_setopt(encoder, SIXEL_OPTFLAG_COLORS, "4");
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    status = sixel_encoder_setopt(encoder, SIXEL_OPTFLAG_DIFFUSION, "none");
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    status = sixel_encoder_setopt(encoder,
                                  SIXEL_OPTFLAG_TRANSPARENT_POLICY,
                                  "keep");
    if (SIXEL_FAILED(status)) {
        goto end;
    }

    status = accumulation_make_rgba_frame(allocator, first, &frame);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    status = sixel_output_new(&output,
                              accumulation_write,
                              &payload,
                              allocator);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    status = sixel_encoder_encode_frame(encoder, frame, output);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    sixel_output_unref(output);
    output = NULL;
    sixel_frame_unref(frame);
    frame = NULL;
    memset(&payload, 0, sizeof(payload));

    status = accumulation_make_rgba_frame(allocator, second, &frame);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    status = sixel_output_new(&output,
                              accumulation_write,
                              &payload,
                              allocator);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    status = sixel_encoder_encode_frame(encoder, frame, output);
    if (SIXEL_FAILED(status)) {
        goto end;
    }

    options.preferred_pixelformat = SIXEL_PIXELFORMAT_RGBA8888;
    status = sixel_decode_pixels(payload.bytes,
                                  (size_t)payload.size,
                                  &options,
                                  &result,
                                  allocator);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    if (result.pixels == NULL || result.width <= 0 ||
        result.height <= 0 || result.stride < 4) {
        status = SIXEL_RUNTIME_ERROR;
        goto end;
    }
    *alpha_out = result.pixels[3];
    status = SIXEL_OK;

end:
    if (result.pixels != NULL) {
        sixel_allocator_free(allocator, result.pixels);
    }
    if (output != NULL) {
        sixel_output_unref(output);
    }
    sixel_frame_unref(frame);
    if (encoder != NULL) {
        sixel_encoder_unref(encoder);
    }
    return status;
}

int
test_filter_0011_filter_encode_accumulation_buffer(int argc, char **argv)
{
    SIXELSTATUS status;
    sixel_allocator_t *allocator;
    unsigned char previous[ACCUMULATION_BYTES];
    unsigned char current[ACCUMULATION_BYTES];
    unsigned char near_previous[ACCUMULATION_BYTES];
    unsigned char near_current[ACCUMULATION_BYTES];
    unsigned char near_next[ACCUMULATION_BYTES];
    unsigned char hidden_first[ACCUMULATION_RGBA_BYTES];
    unsigned char hidden_second[ACCUMULATION_RGBA_BYTES];
    unsigned char hidden_second_alpha;
    int index;
    int changed_index;
    int full_size;
    int accumulation_size;
    int auto_first_size;
    int auto_second_size;
    int near_without_delta_size;
    int near_below_delta_size;
    int near_with_delta_size;
    int delta_first_size;
    int delta_second_size;
    int delta_third_size;
    int full_has_keep_header;
    int accumulation_has_keep_header;
    int auto_second_has_keep_header;
    int near_has_keep_header;
    int ok;

    (void)argc;
    (void)argv;

    status = SIXEL_FALSE;
    allocator = NULL;
    index = 0;
    changed_index = 0;
    full_size = 0;
    accumulation_size = 0;
    auto_first_size = 0;
    auto_second_size = 0;
    near_without_delta_size = 0;
    near_below_delta_size = 0;
    near_with_delta_size = 0;
    delta_first_size = 0;
    delta_second_size = 0;
    delta_third_size = 0;
    hidden_second_alpha = 0u;
    full_has_keep_header = 0;
    accumulation_has_keep_header = 0;
    auto_second_has_keep_header = 0;
    near_has_keep_header = 0;
    ok = 0;

    for (index = 0; index < ACCUMULATION_PIXELS; ++index) {
        previous[index * 3 + 0] = (index & 1) == 0 ? 255u : 0u;
        previous[index * 3 + 1] = 0u;
        previous[index * 3 + 2] = (index & 1) == 0 ? 0u : 255u;
        current[index * 3 + 0] = previous[index * 3 + 0];
        current[index * 3 + 1] = 0u;
        current[index * 3 + 2] = previous[index * 3 + 2];
        near_previous[index * 3 + 0] = 100u;
        near_previous[index * 3 + 1] = 100u;
        near_previous[index * 3 + 2] = 100u;
        near_current[index * 3 + 0] = 104u;
        near_current[index * 3 + 1] = 104u;
        near_current[index * 3 + 2] = 104u;
        near_next[index * 3 + 0] = 108u;
        near_next[index * 3 + 1] = 108u;
        near_next[index * 3 + 2] = 108u;
        hidden_first[index * 4 + 0] = 100u;
        hidden_first[index * 4 + 1] = 100u;
        hidden_first[index * 4 + 2] = 100u;
        hidden_first[index * 4 + 3] = 0u;
        hidden_second[index * 4 + 0] = 100u;
        hidden_second[index * 4 + 1] = 100u;
        hidden_second[index * 4 + 2] = 100u;
        hidden_second[index * 4 + 3] = 255u;
    }
    changed_index = (ACCUMULATION_HEIGHT / 2) * ACCUMULATION_WIDTH
        + (ACCUMULATION_WIDTH / 2);
    current[changed_index * 3 + 0] = 0u;
    current[changed_index * 3 + 1] = 255u;
    current[changed_index * 3 + 2] = 0u;

    status = sixel_allocator_new(&allocator, NULL, NULL, NULL, NULL);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    status = accumulation_encode(allocator,
                                 NULL,
                                 current,
                                 NULL,
                                 &full_size,
                                 &full_has_keep_header);
    if (SIXEL_FAILED(status)) {
        fprintf(stderr, "full frame encode failed: %04x\n", status);
        goto end;
    }
    status = accumulation_encode(allocator,
                                 previous,
                                 current,
                                 NULL,
                                 &accumulation_size,
                                 &accumulation_has_keep_header);
    if (SIXEL_FAILED(status)) {
        fprintf(stderr, "accumulation encode failed: %04x\n", status);
        goto end;
    }
    if (full_has_keep_header != 0) {
        fprintf(stderr, "non-accumulation encode unexpectedly used P2=1\n");
        goto end;
    }
    if (accumulation_has_keep_header == 0) {
        fprintf(stderr, "accumulation encode did not use P2=1\n");
        goto end;
    }
    if (accumulation_size >= full_size) {
        fprintf(stderr,
                "accumulation output not smaller (%d >= %d)\n",
                accumulation_size,
                full_size);
        goto end;
    }
    status = accumulation_encode_sequence(allocator,
                                          previous,
                                          current,
                                          &auto_first_size,
                                          &auto_second_size,
                                          &auto_second_has_keep_header);
    if (SIXEL_FAILED(status)) {
        fprintf(stderr, "auto accumulation encode failed: %04x\n", status);
        goto end;
    }
    if (auto_second_has_keep_header == 0) {
        fprintf(stderr, "auto accumulation did not use P2=1\n");
        goto end;
    }
    if (auto_second_size >= auto_first_size) {
        fprintf(stderr,
                "auto accumulation output not smaller (%d >= %d)\n",
                auto_second_size,
                auto_first_size);
        goto end;
    }
    status = accumulation_encode(allocator,
                                 near_previous,
                                 near_current,
                                 NULL,
                                 &near_without_delta_size,
                                 &near_has_keep_header);
    if (SIXEL_FAILED(status)) {
        fprintf(stderr, "near accumulation encode failed: %04x\n", status);
        goto end;
    }
    status = accumulation_encode(allocator,
                                 near_previous,
                                 near_current,
                                 "3",
                                 &near_below_delta_size,
                                 &near_has_keep_header);
    if (SIXEL_FAILED(status)) {
        fprintf(stderr,
                "near accumulation below-delta encode failed: %04x\n",
                status);
        goto end;
    }
    status = accumulation_encode(allocator,
                                 near_previous,
                                 near_current,
                                 "4",
                                 &near_with_delta_size,
                                 &near_has_keep_header);
    if (SIXEL_FAILED(status)) {
        fprintf(stderr,
                "near 6delta threshold encode failed: %04x\n",
                status);
        goto end;
    }
    if (near_below_delta_size <= near_with_delta_size) {
        fprintf(stderr,
                "below-threshold delta unexpectedly retained near pixels "
                "(%d <= %d)\n",
                near_below_delta_size,
                near_with_delta_size);
        goto end;
    }
    if (near_with_delta_size >= near_without_delta_size) {
        fprintf(stderr,
                "6delta threshold did not reduce output (%d >= %d)\n",
                near_with_delta_size,
                near_without_delta_size);
        goto end;
    }
    status = accumulation_encode_sequence_delta3(allocator,
                                                 near_previous,
                                                 near_current,
                                                 near_next,
                                                 "4",
                                                 &delta_first_size,
                                                 &delta_second_size,
                                                 &delta_third_size);
    if (SIXEL_FAILED(status)) {
        fprintf(stderr,
                "auto 6delta threshold sequence failed: %04x\n",
                status);
        goto end;
    }
    if (delta_second_size >= delta_first_size) {
        fprintf(stderr,
                "second delta frame was not retained (%d >= %d)\n",
                delta_second_size,
                delta_first_size);
        goto end;
    }
    if (delta_third_size <= delta_second_size) {
        fprintf(stderr,
                "delta accumulation advanced past displayed plane "
                "(%d <= %d)\n",
                delta_third_size,
                delta_second_size);
        goto end;
    }
    status = accumulation_hidden_alpha_seed_second_alpha(
        allocator,
        hidden_first,
        hidden_second,
        &hidden_second_alpha);
    if (SIXEL_FAILED(status)) {
        fprintf(stderr,
                "hidden alpha seed encode failed: %04x\n",
                status);
        goto end;
    }
    if (hidden_second_alpha == 0u) {
        fprintf(stderr,
                "hidden transparent RGB was retained as displayed plane\n");
        goto end;
    }

    ok = 1;

end:
    if (allocator != NULL) {
        sixel_allocator_unref(allocator);
    }
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
