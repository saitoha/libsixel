/*
 * SPDX-License-Identifier: MIT
 *
 * A pixel that goes black -> white -> black has to come back black.
 *
 * Transparency is carried by an entry appended to the palette, and its RGB
 * defaults to black.  While that entry took part in the nearest-color lookup,
 * any black or near-black pixel resolved to it and was emitted as the
 * transparent index, so a P2=1 terminal kept showing whatever was there
 * before -- the previous frame's white stayed on screen as a ghost.
 *
 * The check composites each frame the way such a terminal would and requires
 * the black frames to actually be painted.
 */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <6cells.h>
#include <sixel.h>

#include "src/compat_stub.h"

#define KEY_WIDTH 64
#define KEY_HEIGHT 32
#define KEY_BOX_X0 8
#define KEY_BOX_Y0 8
#define KEY_BOX_X1 56
#define KEY_BOX_Y1 24
#define KEY_OUTPUT_CAPACITY 65536

typedef struct key_payload {
    unsigned char bytes[KEY_OUTPUT_CAPACITY];
    int size;
} key_payload_t;

static int
key_write(char *data, int size, void *priv)
{
    key_payload_t *payload;

    payload = (key_payload_t *)priv;
    if (data == NULL || payload == NULL || size < 0) {
        return 1;
    }
    if (size > KEY_OUTPUT_CAPACITY - payload->size) {
        return 1;
    }
    memcpy(payload->bytes + payload->size, data, (size_t)size);
    payload->size += size;

    return 0;
}

/*
 * Static surroundings so the palette stays put; only the box changes, and it
 * alternates between the two extremes of the gamut.
 */
static void
key_fill(unsigned char *pixels, int box_white)
{
    int x;
    int y;
    size_t offset;
    int value;

    for (y = 0; y < KEY_HEIGHT; ++y) {
        for (x = 0; x < KEY_WIDTH; ++x) {
            offset = ((size_t)y * KEY_WIDTH + (size_t)x) * 3u;
            value = 60 + ((x * 3 + y * 5) % 120);
            pixels[offset] = (unsigned char)value;
            pixels[offset + 1u] = (unsigned char)(value / 2 + 40);
            pixels[offset + 2u] = (unsigned char)(200 - value / 2);
        }
    }
    for (y = KEY_BOX_Y0; y < KEY_BOX_Y1; ++y) {
        for (x = KEY_BOX_X0; x < KEY_BOX_X1; ++x) {
            offset = ((size_t)y * KEY_WIDTH + (size_t)x) * 3u;
            pixels[offset] = (unsigned char)(box_white ? 0xffu : 0x00u);
            pixels[offset + 1u] = pixels[offset];
            pixels[offset + 2u] = pixels[offset];
        }
    }
}

/* Encode one frame and composite it onto CANVAS with P2=1 semantics. */
static SIXELSTATUS
key_encode_frame(sixel_encoder_t *encoder,
                 sixel_allocator_t *allocator,
                 unsigned char *canvas,
                 int box_white,
                 int frame_no,
                 long *kept_out)
{
    SIXELSTATUS status;
    sixel_frame_t *frame;
    sixel_output_t *output;
    key_payload_t payload;
    unsigned char *pixels;
    unsigned char *decoded;
    unsigned char *palette;
    int decoded_width;
    int decoded_height;
    int ncolors;
    int x;
    int y;

    status = SIXEL_FALSE;
    frame = NULL;
    output = NULL;
    decoded = NULL;
    palette = NULL;
    decoded_width = 0;
    decoded_height = 0;
    ncolors = 0;
    memset(&payload, 0, sizeof(payload));
    if (kept_out != NULL) {
        *kept_out = 0;
    }

    pixels = (unsigned char *)sixel_allocator_malloc(
        allocator,
        (size_t)KEY_WIDTH * KEY_HEIGHT * 3u);
    if (pixels == NULL) {
        return SIXEL_BAD_ALLOCATION;
    }
    key_fill(pixels, box_white);

    status = sixel_frame_new(&frame, allocator);
    if (SIXEL_FAILED(status)) {
        sixel_allocator_free(allocator, pixels);
        goto end;
    }
    status = sixel_frame_init(frame,
                              pixels,
                              KEY_WIDTH,
                              KEY_HEIGHT,
                              SIXEL_PIXELFORMAT_RGB888,
                              NULL,
                              -1);
    if (SIXEL_FAILED(status)) {
        sixel_allocator_free(allocator, pixels);
        goto end;
    }
    sixel_frame_set_multiframe(frame, 0);
    sixel_frame_set_frame_no(frame, frame_no);

    status = sixel_output_new(&output, key_write, &payload, allocator);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    status = sixel_encoder_encode_frame(encoder, frame, output);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    status = sixel_decode_raw(payload.bytes,
                              payload.size,
                              &decoded,
                              &decoded_width,
                              &decoded_height,
                              &palette,
                              &ncolors,
                              allocator);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    for (y = 0; y < decoded_height && y < KEY_HEIGHT; ++y) {
        for (x = 0; x < decoded_width && x < KEY_WIDTH; ++x) {
            int index;
            size_t canvas_offset;

            index = (int)decoded[(size_t)y * decoded_width + x];
            if (index >= ncolors) {
                if (kept_out != NULL) {
                    (*kept_out)++;
                }
                continue; /* transparent: the canvas keeps its old color */
            }
            canvas_offset = ((size_t)y * KEY_WIDTH + (size_t)x) * 3u;
            canvas[canvas_offset] = palette[index * 3];
            canvas[canvas_offset + 1u] = palette[index * 3 + 1];
            canvas[canvas_offset + 2u] = palette[index * 3 + 2];
        }
    }
    status = SIXEL_OK;

end:
    if (decoded != NULL) {
        sixel_allocator_free(allocator, decoded);
    }
    if (palette != NULL) {
        sixel_allocator_free(allocator, palette);
    }
    if (output != NULL) {
        sixel_output_unref(output);
    }
    if (frame != NULL) {
        sixel_frame_unref(frame);
    }
    return status;
}

/* Mean of the box area on the canvas, i.e. what the terminal is showing. */
static int
key_box_mean(unsigned char const *canvas)
{
    long total;
    long count;
    int x;
    int y;

    total = 0;
    count = 0;
    for (y = KEY_BOX_Y0; y < KEY_BOX_Y1; ++y) {
        for (x = KEY_BOX_X0; x < KEY_BOX_X1; ++x) {
            size_t offset;

            offset = ((size_t)y * KEY_WIDTH + (size_t)x) * 3u;
            total += canvas[offset];
            total += canvas[offset + 1u];
            total += canvas[offset + 2u];
            count += 3;
        }
    }
    if (count == 0) {
        return -1;
    }

    return (int)(total / count);
}

int
test_filter_0013_filter_encode_6delta_keycolor(int argc, char **argv)
{
    static int const sequence[] = { 0, 1, 0, 1, 0 };
    sixel_allocator_t *allocator;
    sixel_encoder_t *encoder;
    unsigned char *canvas;
    size_t index;
    int mean;
    int ok;

    (void)argc;
    (void)argv;
    allocator = NULL;
    encoder = NULL;
    canvas = NULL;
    ok = 0;

    if (SIXEL_FAILED(sixel_allocator_new(&allocator, NULL, NULL, NULL, NULL))) {
        return EXIT_FAILURE;
    }
    canvas = (unsigned char *)sixel_allocator_malloc(
        allocator,
        (size_t)KEY_WIDTH * KEY_HEIGHT * 3u);
    if (canvas == NULL) {
        goto end;
    }
    memset(canvas, 0x7f, (size_t)KEY_WIDTH * KEY_HEIGHT * 3u);

    if (SIXEL_FAILED(sixel_encoder_new(&encoder, allocator))) {
        goto end;
    }
    if (SIXEL_FAILED(sixel_encoder_setopt(encoder, SIXEL_OPTFLAG_COLORS, "64"))
        || SIXEL_FAILED(sixel_encoder_setopt(encoder,
                                             SIXEL_OPTFLAG_DIFFUSION,
                                             "none"))
        || SIXEL_FAILED(sixel_encoder_setopt(encoder,
                                             SIXEL_OPTFLAG_ALPHA_POLICY,
                                             "keep"))
        || SIXEL_FAILED(sixel_encoder_setopt(encoder,
                                             SIXEL_OPTFLAG_UPDATE_POLICY,
                                             "delta:threshold=0"))) {
        fprintf(stderr, "encoder setup failed\n");
        goto end;
    }

    for (index = 0u; index < sizeof(sequence) / sizeof(sequence[0]); ++index) {
        if (SIXEL_FAILED(key_encode_frame(encoder,
                                          allocator,
                                          canvas,
                                          sequence[index],
                                          (int)index + 1,
                                          NULL))) {
            fprintf(stderr, "frame %lu failed\n", (unsigned long)index);
            goto end;
        }
        mean = key_box_mean(canvas);
        if (sequence[index] != 0) {
            if (mean < 200) {
                fprintf(stderr,
                        "frame %lu wanted white, terminal shows %d\n",
                        (unsigned long)index,
                        mean);
                goto end;
            }
        } else if (mean > 55) {
            fprintf(stderr,
                    "frame %lu wanted black, terminal shows %d -- a black "
                    "pixel resolved to the transparency key\n",
                    (unsigned long)index,
                    mean);
            goto end;
        }
    }
    /*
     * Threshold 0 used to mean "keep only on a bit-exact match", which a
     * quantized palette essentially never produces, so 6delta did nothing at
     * all by default.  Keeping is now decided by comparing against the entry
     * the lookup picked, so an unchanged frame has to be kept even at 0.
     */
    {
        long kept;

        kept = 0;
        if (SIXEL_FAILED(key_encode_frame(encoder,
                                          allocator,
                                          canvas,
                                          sequence[(sizeof(sequence)
                                                    / sizeof(sequence[0])) - 1],
                                          (int)(sizeof(sequence)
                                                / sizeof(sequence[0])) + 1,
                                          &kept))) {
            fprintf(stderr, "repeat frame failed\n");
            goto end;
        }
        if (kept == 0) {
            fprintf(stderr,
                    "an unchanged frame kept nothing at threshold 0\n");
            goto end;
        }
    }
    ok = 1;

end:
    if (encoder != NULL) {
        sixel_encoder_unref(encoder);
    }
    if (canvas != NULL) {
        sixel_allocator_free(allocator, canvas);
    }
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
