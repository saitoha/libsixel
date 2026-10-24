/* SPDX-License-Identifier: MIT */
/* GPU darkening must paint real colors and retain the composed wire image. */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sixel.h>
#include "src/encoder.h"
#include "src/sixel_decode_pixels.h"

#define GPU_FADE_WIDTH 32
#define GPU_FADE_HEIGHT 12
#define GPU_FADE_COUNT (GPU_FADE_WIDTH * GPU_FADE_HEIGHT)

typedef struct gpu_fade_stream {
    unsigned char bytes[8192];
    int size;
} gpu_fade_stream_t;

static int
gpu_fade_write(char *bytes, int size, void *context)
{
    gpu_fade_stream_t *stream;

    stream = context;
    if (size < 0 || size > (int)sizeof(stream->bytes) - stream->size) {
        return -1;
    }
    memcpy(stream->bytes + stream->size, bytes, (size_t)size);
    stream->size += size;
    return size;
}

int
test_gpupal_0003_fade_history(int argc, char **argv)
{
    static struct { int flag; char const *value; } const options[] = {
        {SIXEL_OPTFLAG_GPU_POLICY, "force"}, {'=', "greedy:B1"},
        {'.', "8bit"}, {'d', "bluenoise"}, {'~', "none"},
        {'X', "gamma"}, {'W', "gamma"}, {'_', "none"},
        {'a', "off"}, {'F', "none"}, {'Q', "heckbert"},
        {'p', "8"}, {'t', "rgb"}, {'Z', "delta:T8"}
    };
    /* White -> dark ramp -> black -> repeated black. The ramp has more
     * source colors than the palette, so lookup and dither cannot be elided
     * by a lossless/uniform shortcut. Its centroids stay inside [3, 63];
     * RGB101 rounding admits at most 64. White cannot legitimately win a
     * keep against that ramp. Pure black then permits only the T=8 residue.
     * These bounds are independent of the quantizer's palette choices. */
    unsigned char const maximum[] = {255, 64, 8, 8};
    sixel_encoder_t *encoder;
    sixel_frame_t *frame;
    sixel_output_t *output;
    gpu_fade_stream_t stream;
    unsigned char input[GPU_FADE_COUNT * 3];
    unsigned char canvas[GPU_FADE_COUNT * 3];
    unsigned char *decoded, *paint, *palette;
    unsigned int flags;
    SIXELSTATUS status;
    int step, i, width, height, colors, want, ok;

    (void)argc;
    (void)argv;
    encoder = NULL;
    frame = NULL;
    output = NULL;
    decoded = paint = palette = NULL;
    ok = 0;
    memset(canvas, 0, sizeof(canvas));
    status = sixel_encoder_new(&encoder, NULL);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    status = sixel_output_new(&output, gpu_fade_write, &stream, NULL);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    for (i = 0; i < (int)(sizeof(options) / sizeof(options[0])); ++i) {
        status = sixel_encoder_setopt(encoder, options[i].flag,
            options[i].value);
        if (SIXEL_FAILED(status)) {
            goto end;
        }
    }
    for (step = 0; step < 4; ++step) {
        for (i = 0; i < GPU_FADE_COUNT; ++i) {
            want = step == 0 ? 255 : (step == 1 ? 3 + i % 61 : 0);
            memset(input + i * 3, want, 3);
        }
        status = sixel_frame_new(&frame, NULL);
        if (SIXEL_FAILED(status)) {
            goto end;
        }
        status = sixel_frame_init_borrowed(frame, input, GPU_FADE_WIDTH,
            GPU_FADE_HEIGHT, SIXEL_PIXELFORMAT_RGB888, NULL, -1);
        if (SIXEL_FAILED(status)) {
            goto end;
        }
        stream.size = 0;
        status = sixel_encoder_encode_frame(encoder, frame, output);
        if (SIXEL_FAILED(status)) {
            goto end;
        }
        status = sixel_decode_raw_with_options_mask(stream.bytes,
            stream.size, 0, &decoded, &paint, &width, &height, &palette,
            &colors, &flags, encoder->allocator);
        if (SIXEL_FAILED(status) || width != GPU_FADE_WIDTH ||
                height != GPU_FADE_HEIGHT || !encoder->accumulation_valid ||
                encoder->accumulation_pixels_size != sizeof(canvas) ||
                encoder->accumulation_valid_mask_size != GPU_FADE_COUNT) {
            goto end;
        }
        for (i = 0; i < GPU_FADE_COUNT; ++i) {
            if (paint[i]) {
                if (decoded[i] >= colors) {
                    goto end;
                }
                memcpy(canvas + i * 3, palette + decoded[i] * 3, 3);
            }
            if (!encoder->accumulation_valid_mask[i] ||
                    (step == 0 && (canvas[i * 3] != 255 ||
                        canvas[i * 3 + 1] != 255 ||
                        canvas[i * 3 + 2] != 255)) ||
                    canvas[i * 3] > maximum[step] ||
                    canvas[i * 3 + 1] > maximum[step] ||
                    canvas[i * 3 + 2] > maximum[step] ||
                    memcmp(canvas + i * 3,
                        encoder->accumulation_pixels + i * 3, 3)) {
                fprintf(stderr, "GPU fade mismatch at frame %d pixel %d\n",
                    step, i);
                goto end;
            }
        }
        sixel_allocator_free(encoder->allocator, decoded);
        sixel_allocator_free(encoder->allocator, paint);
        sixel_allocator_free(encoder->allocator, palette);
        decoded = paint = palette = NULL;
        sixel_frame_unref(frame);
        frame = NULL;
    }
    ok = 1;

end:
    if (!ok) {
        fprintf(stderr, "GPU fade failed: %s\n",
            sixel_helper_get_additional_message());
    }
    if (encoder != NULL) {
        sixel_allocator_free(encoder->allocator, decoded);
        sixel_allocator_free(encoder->allocator, paint);
        sixel_allocator_free(encoder->allocator, palette);
    }
    if (frame != NULL) {
        sixel_frame_unref(frame);
    }
    if (output != NULL) {
        sixel_output_unref(output);
    }
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
