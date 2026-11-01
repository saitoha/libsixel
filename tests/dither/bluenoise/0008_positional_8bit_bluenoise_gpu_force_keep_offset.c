/* SPDX-License-Identifier: MIT */
/* Verify forced GPU bluenoise with transparent-offset frame reuse. */
#include <sixel.h>

#include <stdio.h>
#include <stdlib.h>

#define BN_KEEP_WIDTH 96
#define BN_KEEP_HEIGHT 72
#define BN_KEEP_PIXEL_COUNT (BN_KEEP_WIDTH * BN_KEEP_HEIGHT)
#define BN_KEEP_BYTES (BN_KEEP_PIXEL_COUNT * 3)

static int
bn_keep_write(char *data, int size, void *priv)
{
    (void)data;
    (void)priv;
    if (size < 0) {
        return 1;
    }
    return 0;
}

static void
bn_keep_fill(unsigned char *pixels, int frame_no)
{
    int x;
    int y;
    size_t offset;

    x = 0;
    y = 0;
    offset = 0u;
    for (y = 0; y < BN_KEEP_HEIGHT; ++y) {
        for (x = 0; x < BN_KEEP_WIDTH; ++x) {
            offset = ((size_t)y * (size_t)BN_KEEP_WIDTH + (size_t)x) * 3u;
            pixels[offset + 0u] =
                (unsigned char)((x * 5 + y * 3 + frame_no * 7) & 0xff);
            pixels[offset + 1u] =
                (unsigned char)((x * 2 + y * 9 + frame_no * 5) & 0xff);
            pixels[offset + 2u] =
                (unsigned char)((x * 11 + y + frame_no * 13) & 0xff);
        }
    }
}

static SIXELSTATUS
bn_keep_encode(sixel_encoder_t *encoder, unsigned char *pixels, int frame_no)
{
    SIXELSTATUS status;
    sixel_frame_t *frame;
    sixel_output_t *output;

    status = SIXEL_FALSE;
    frame = NULL;
    output = NULL;
    if (encoder == NULL || pixels == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    status = sixel_frame_new(&frame, NULL);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    status = sixel_frame_init_borrowed(frame,
                                       pixels,
                                       BN_KEEP_WIDTH,
                                       BN_KEEP_HEIGHT,
                                       SIXEL_PIXELFORMAT_RGB888,
                                       NULL,
                                       (-1));
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    sixel_frame_set_multiframe(frame, frame_no > 0 ? 1 : 0);

    status = sixel_output_new(&output, bn_keep_write, NULL, NULL);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    status = sixel_encoder_encode_frame(encoder, frame, output);

end:
    if (output != NULL) {
        sixel_output_unref(output);
    }
    if (frame != NULL) {
        sixel_frame_unref(frame);
    }
    return status;
}

int
test_bluenoise_0052_keep_offset(int argc, char **argv)
{
    SIXELSTATUS status;
    sixel_encoder_t *encoder;
    unsigned char *pixels;
    int frame_no;
    int ok;

    (void)argc;
    (void)argv;
    status = SIXEL_FALSE;
    encoder = NULL;
    pixels = NULL;
    frame_no = 0;
    ok = 0;

    status = sixel_encoder_new(&encoder, NULL);
    if (SIXEL_FAILED(status)) {
        fprintf(stderr, "encoder allocation failed: %04x\n", status);
        goto end;
    }
    status = sixel_encoder_setopt(encoder, SIXEL_OPTFLAG_COLORS, "256");
    if (SIXEL_FAILED(status)) {
        fprintf(stderr, "colors option failed: %04x\n", status);
        goto end;
    }
    status = sixel_encoder_setopt(encoder, SIXEL_OPTFLAG_DIFFUSION,
                                  "bluenoise");
    if (SIXEL_FAILED(status)) {
        fprintf(stderr, "diffusion option failed: %04x\n", status);
        goto end;
    }
    status = sixel_encoder_setopt(encoder, SIXEL_OPTFLAG_LUT_POLICY, "none");
    if (SIXEL_FAILED(status)) {
        fprintf(stderr, "lookup option failed: %04x\n", status);
        goto end;
    }
    status = sixel_encoder_setopt(encoder, SIXEL_OPTFLAG_GPU_POLICY, "force");
    if (SIXEL_FAILED(status)) {
        fprintf(stderr, "gpu option failed: %04x\n", status);
        goto end;
    }
    status = sixel_encoder_setopt(encoder,
                                  SIXEL_OPTFLAG_ALPHA_POLICY,
                                  "keep");
    if (SIXEL_FAILED(status)) {
        fprintf(stderr, "alpha policy option failed: %04x\n", status);
        goto end;
    }
    status = sixel_encoder_setopt(encoder,
                                  SIXEL_OPTFLAG_TRANSPARENT_OFFSET,
                                  "15,35");
    if (SIXEL_FAILED(status)) {
        fprintf(stderr, "transparent offset option failed: %04x\n", status);
        goto end;
    }

    pixels = (unsigned char *)malloc(BN_KEEP_BYTES);
    if (pixels == NULL) {
        fprintf(stderr, "pixel allocation failed\n");
        goto end;
    }

    for (frame_no = 0; frame_no < 3; ++frame_no) {
        bn_keep_fill(pixels, frame_no);
        status = bn_keep_encode(encoder, pixels, frame_no);
        if (SIXEL_FAILED(status)) {
            fprintf(stderr, "frame %d encode failed: %04x\n",
                    frame_no,
                    status);
            goto end;
        }
    }

    ok = 1;

end:
    free(pixels);
    if (encoder != NULL) {
        sixel_encoder_unref(encoder);
    }
    return ok != 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
