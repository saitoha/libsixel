/*
 * SPDX-License-Identifier: MIT
 *
 * Verify that 6delta keeps survive a damage rectangle that moves between
 * frames.  A caller that repaints sub-rectangles of a larger surface declares
 * the surface once with sixel_encoder_set_6delta_plane_size() and places each
 * frame with sixel_encoder_set_6delta_plane_origin().  Without the retained
 * plane, every frame geometry change discards the history and the encoder
 * repaints pixels the terminal already shows.
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
#include "src/encoder.h"

#define PLANE_WIDTH 48
#define PLANE_HEIGHT 24
#define FRAME_WIDTH 24
#define FRAME_HEIGHT 12
#define PLANE_OUTPUT_CAPACITY 65536

typedef struct plane_payload {
    unsigned char bytes[PLANE_OUTPUT_CAPACITY];
    int size;
} plane_payload_t;

static int
plane_write(char *data, int size, void *priv)
{
    plane_payload_t *payload;
    int room;

    payload = (plane_payload_t *)priv;
    if (data == NULL || payload == NULL || size < 0) {
        return 1;
    }
    room = PLANE_OUTPUT_CAPACITY - payload->size;
    if (size > room) {
        return 1;
    }
    memcpy(payload->bytes + payload->size, data, (size_t)size);
    payload->size += size;

    return 0;
}

/*
 * The surface is four horizontal colour bands, six plane rows each, so a
 * pixel's colour is a function of its position on the plane rather than its
 * position inside the frame.  Cutting a frame at a different origin therefore
 * yields different frame-local content but identical plane content in the
 * overlap, which is exactly what frame-shaped history cannot express.
 */
static void
plane_fill_frame(unsigned char *rgb, int origin_x, int origin_y)
{
    static unsigned char const bands[4][3] = {
        { 0xffu, 0x00u, 0x00u },
        { 0x00u, 0xffu, 0x00u },
        { 0x00u, 0x00u, 0xffu },
        { 0xffu, 0xffu, 0x00u }
    };
    int x;
    int y;
    int band;
    size_t index;

    (void)origin_x;
    for (y = 0; y < FRAME_HEIGHT; ++y) {
        band = ((origin_y + y) / 6) % 4;
        for (x = 0; x < FRAME_WIDTH; ++x) {
            index = ((size_t)y * FRAME_WIDTH + (size_t)x) * 3u;
            rgb[index] = bands[band][0];
            rgb[index + 1u] = bands[band][1];
            rgb[index + 2u] = bands[band][2];
        }
    }
}

/*
 * Encode one frame at ORIGIN_X,ORIGIN_Y and report how many pixels came back
 * transparent, which is what a P2=1 terminal leaves untouched.
 */
static SIXELSTATUS
plane_encode_frame(sixel_encoder_t *encoder,
                   int origin_x,
                   int origin_y,
                   int declare_plane,
                   long *kept_out)
{
    SIXELSTATUS status;
    sixel_frame_t *frame;
    sixel_output_t *output;
    sixel_allocator_t *allocator;
    plane_payload_t payload;
    unsigned char *pixels;
    unsigned char *decoded;
    unsigned char *palette;
    int decoded_width;
    int decoded_height;
    int ncolors;
    int index;
    long kept;

    status = SIXEL_FALSE;
    frame = NULL;
    output = NULL;
    allocator = NULL;
    decoded = NULL;
    palette = NULL;
    pixels = NULL;
    decoded_width = 0;
    decoded_height = 0;
    ncolors = 0;
    kept = 0;
    memset(&payload, 0, sizeof(payload));
    if (kept_out != NULL) {
        *kept_out = 0;
    }

    status = sixel_allocator_new(&allocator, NULL, NULL, NULL, NULL);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    pixels = (unsigned char *)sixel_allocator_malloc(
        allocator,
        (size_t)FRAME_WIDTH * FRAME_HEIGHT * 3u);
    if (pixels == NULL) {
        status = SIXEL_BAD_ALLOCATION;
        goto end;
    }
    plane_fill_frame(pixels, origin_x, origin_y);

    status = sixel_frame_new(&frame, allocator);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    status = sixel_frame_init(frame,
                              pixels,
                              FRAME_WIDTH,
                              FRAME_HEIGHT,
                              SIXEL_PIXELFORMAT_RGB888,
                              NULL,
                              -1);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    pixels = NULL;
    sixel_frame_set_multiframe(frame, 0);

    status = sixel_output_new(&output, plane_write, &payload, allocator);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    if (declare_plane) {
        status = sixel_encoder_set_6delta_plane_origin(encoder,
                                                       origin_x,
                                                       origin_y);
        if (SIXEL_FAILED(status)) {
            goto end;
        }
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
    for (index = 0; index < decoded_width * decoded_height; ++index) {
        if ((int)decoded[index] >= ncolors) {
            kept++;
        }
    }
    if (kept_out != NULL) {
        *kept_out = kept;
    }
    status = SIXEL_OK;

end:
    if (decoded != NULL) {
        sixel_allocator_free(allocator, decoded);
    }
    if (palette != NULL) {
        sixel_allocator_free(allocator, palette);
    }
    if (pixels != NULL) {
        sixel_allocator_free(allocator, pixels);
    }
    if (output != NULL) {
        sixel_output_unref(output);
    }
    if (frame != NULL) {
        sixel_frame_unref(frame);
    }
    if (allocator != NULL) {
        sixel_allocator_unref(allocator);
    }
    return status;
}

static SIXELSTATUS
plane_make_encoder(sixel_encoder_t **encoder_out, int declare_plane)
{
    SIXELSTATUS status;
    sixel_encoder_t *encoder;

    encoder = NULL;
    status = sixel_encoder_new(&encoder, NULL);
    if (SIXEL_FAILED(status)) {
        return status;
    }
    status = sixel_encoder_setopt(encoder, SIXEL_OPTFLAG_COLORS, "16");
    if (SIXEL_SUCCEEDED(status)) {
        status = sixel_encoder_setopt(encoder,
                                      SIXEL_OPTFLAG_DIFFUSION,
                                      "none");
    }
    if (SIXEL_SUCCEEDED(status)) {
        status = sixel_encoder_setopt(encoder,
                                      SIXEL_OPTFLAG_ALPHA_POLICY,
                                      "keep");
    }
    if (SIXEL_SUCCEEDED(status)) {
        status = sixel_encoder_setopt(encoder,
                                      SIXEL_OPTFLAG_6DELTA_THRESHOLD,
                                      "16");
    }
    if (SIXEL_SUCCEEDED(status) && declare_plane) {
        status = sixel_encoder_set_6delta_plane_size(encoder,
                                                     PLANE_WIDTH,
                                                     PLANE_HEIGHT);
    }
    if (SIXEL_FAILED(status)) {
        sixel_encoder_unref(encoder);
        return status;
    }
    *encoder_out = encoder;

    return SIXEL_OK;
}

int
test_filter_0012_filter_encode_6delta_plane(int argc, char **argv)
{
    SIXELSTATUS status;
    sixel_encoder_t *encoder;
    long kept_first;
    long kept_repeat;
    long kept_moved;
    long kept_no_plane;
    long kept_after_invalidate;
    int ok;

    (void)argc;
    (void)argv;
    status = SIXEL_FALSE;
    encoder = NULL;
    kept_first = 0;
    kept_repeat = 0;
    kept_moved = 0;
    kept_no_plane = 0;
    kept_after_invalidate = 0;
    ok = 0;

    /* With a declared plane, a moved rectangle still keeps. */
    status = plane_make_encoder(&encoder, 1);
    if (SIXEL_FAILED(status)) {
        fprintf(stderr, "plane encoder setup failed: %04x\n", status);
        goto end;
    }
    status = plane_encode_frame(encoder, 0, 0, 1, &kept_first);
    if (SIXEL_FAILED(status)) {
        fprintf(stderr, "first plane frame failed: %04x\n", status);
        goto end;
    }
    if (kept_first != 0) {
        fprintf(stderr,
                "first frame kept %ld pixels with no history\n",
                kept_first);
        goto end;
    }
    status = plane_encode_frame(encoder, 0, 0, 1, &kept_repeat);
    if (SIXEL_FAILED(status)) {
        fprintf(stderr, "repeated plane frame failed: %04x\n", status);
        goto end;
    }
    if (kept_repeat == 0) {
        fprintf(stderr, "repeated frame at same origin kept nothing\n");
        goto end;
    }

    /*
     * The interesting case: a rectangle that moves so it partly covers plane
     * pixels the earlier frames already painted.  Only the overlap may be
     * kept; the newly exposed area has never been sent and must be painted.
     * A blanket keep or a blanket repaint both fail this.
     */
    status = plane_encode_frame(encoder, 12, 6, 1, &kept_moved);
    if (SIXEL_FAILED(status)) {
        fprintf(stderr, "moved plane frame failed: %04x\n", status);
        goto end;
    }
    if (kept_moved == 0) {
        fprintf(stderr, "moved rectangle kept nothing\n");
        goto end;
    }
    if (kept_moved >= (long)FRAME_WIDTH * FRAME_HEIGHT) {
        fprintf(stderr,
                "moved rectangle kept never-painted pixels: %ld\n",
                kept_moved);
        goto end;
    }

    /* Invalidation must force a full repaint again. */
    status = sixel_encoder_invalidate_6delta_plane(encoder);
    if (SIXEL_FAILED(status)) {
        fprintf(stderr, "plane invalidate failed: %04x\n", status);
        goto end;
    }
    status = plane_encode_frame(encoder, 12, 6, 1, &kept_after_invalidate);
    if (SIXEL_FAILED(status)) {
        fprintf(stderr, "post-invalidate frame failed: %04x\n", status);
        goto end;
    }
    if (kept_after_invalidate != 0) {
        fprintf(stderr,
                "invalidated plane still kept %ld pixels\n",
                kept_after_invalidate);
        goto end;
    }
    sixel_encoder_unref(encoder);
    encoder = NULL;

    /*
     * Without a declared plane the history is frame-shaped, so the same moved
     * rectangle keeps nothing.  This pins the behaviour the plane API fixes.
     */
    status = plane_make_encoder(&encoder, 0);
    if (SIXEL_FAILED(status)) {
        fprintf(stderr, "default encoder setup failed: %04x\n", status);
        goto end;
    }
    status = plane_encode_frame(encoder, 0, 0, 0, &kept_no_plane);
    if (SIXEL_FAILED(status)) {
        fprintf(stderr, "default first frame failed: %04x\n", status);
        goto end;
    }
    status = plane_encode_frame(encoder, 12, 6, 0, &kept_no_plane);
    if (SIXEL_FAILED(status)) {
        fprintf(stderr, "default moved frame failed: %04x\n", status);
        goto end;
    }
    if (kept_no_plane != 0) {
        fprintf(stderr,
                "frame-shaped history unexpectedly kept %ld pixels\n",
                kept_no_plane);
        goto end;
    }

    ok = 1;

end:
    if (encoder != NULL) {
        sixel_encoder_unref(encoder);
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
