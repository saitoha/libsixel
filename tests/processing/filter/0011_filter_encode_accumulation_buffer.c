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

#include <sixel.h>

#define ACCUMULATION_WIDTH 18
#define ACCUMULATION_HEIGHT 18
#define ACCUMULATION_PIXELS (ACCUMULATION_WIDTH * ACCUMULATION_HEIGHT)
#define ACCUMULATION_BYTES (ACCUMULATION_PIXELS * 3)
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
accumulation_encode(sixel_allocator_t *allocator,
                    unsigned char const *previous,
                    unsigned char const *current,
                    char const *accumulation_delta,
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
    if (accumulation_delta != NULL) {
        status = sixel_encoder_setopt(encoder,
                                      SIXEL_OPTFLAG_ACCUMULATION_DELTA,
                                      accumulation_delta);
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

int
test_filter_0011_filter_encode_accumulation_buffer(int argc, char **argv)
{
    SIXELSTATUS status;
    sixel_allocator_t *allocator;
    unsigned char previous[ACCUMULATION_BYTES];
    unsigned char current[ACCUMULATION_BYTES];
    unsigned char near_previous[ACCUMULATION_BYTES];
    unsigned char near_current[ACCUMULATION_BYTES];
    int index;
    int changed_index;
    int full_size;
    int accumulation_size;
    int near_without_delta_size;
    int near_with_delta_size;
    int full_has_keep_header;
    int accumulation_has_keep_header;
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
    near_without_delta_size = 0;
    near_with_delta_size = 0;
    full_has_keep_header = 0;
    accumulation_has_keep_header = 0;
    near_has_keep_header = 0;
    ok = 0;

    for (index = 0; index < ACCUMULATION_PIXELS; ++index) {
        previous[index * 3 + 0] = 255u;
        previous[index * 3 + 1] = 0u;
        previous[index * 3 + 2] = 0u;
        current[index * 3 + 0] = 255u;
        current[index * 3 + 1] = 0u;
        current[index * 3 + 2] = 0u;
        near_previous[index * 3 + 0] = 100u;
        near_previous[index * 3 + 1] = 100u;
        near_previous[index * 3 + 2] = 100u;
        near_current[index * 3 + 0] = 104u;
        near_current[index * 3 + 1] = 104u;
        near_current[index * 3 + 2] = 104u;
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
                                 "4",
                                 &near_with_delta_size,
                                 &near_has_keep_header);
    if (SIXEL_FAILED(status)) {
        fprintf(stderr,
                "near accumulation delta encode failed: %04x\n",
                status);
        goto end;
    }
    if (near_with_delta_size >= near_without_delta_size) {
        fprintf(stderr,
                "accumulation delta did not reduce output (%d >= %d)\n",
                near_with_delta_size,
                near_without_delta_size);
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
