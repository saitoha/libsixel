/* SPDX-License-Identifier: MIT */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sixel.h>
#include "src/encoder.h"
#include "src/encoder-core-private.h"
#include "src/gpu-palette.h"

typedef struct indexed_stream {
    unsigned char data[32768];
    size_t size;
} indexed_stream_t;

static int
indexed_stream_write(char *bytes, int size, void *context)
{
    indexed_stream_t *stream;

    stream = context;
    if (size < 0 || (size_t)size > sizeof(stream->data) - stream->size) {
        return -1;
    }
    memcpy(stream->data + stream->size, bytes, (size_t)size);
    stream->size += (size_t)size;
    return size;
}

int
indexed_equivalence_test(char const *dither, int gpu, int hls, int history)
{
    sixel_encoder_t *encoder[2];
    sixel_output_t *output[2];
    sixel_frame_t *frame;
    indexed_stream_t stream[2];
    unsigned char pixels[16 * 16 * 3];
    SIXELSTATUS status;
    int mode;
    int sequence;
    int i;
    int ok;

    memset(encoder, 0, sizeof(encoder));
    memset(output, 0, sizeof(output));
    memset(stream, 0, sizeof(stream));
    frame = NULL;
    ok = 0;
    mode = sequence = 0;
    status = SIXEL_OK;
    if (gpu && !sixel_gpu_palette_plan_available()) return 77;
    for (mode = 0; mode < 2; ++mode) {
        if (sixel_encoder_new(&encoder[mode], NULL) != SIXEL_OK ||
                sixel_output_new(&output[mode], indexed_stream_write,
                    &stream[mode], NULL) != SIXEL_OK) goto end;
        output[mode]->indexed_disabled = mode;
        if (sixel_encoder_setopt(encoder[mode], 'G', gpu ? "force" : "off") !=
                SIXEL_OK ||
                sixel_encoder_setopt(encoder[mode], 'd', dither) != SIXEL_OK ||
                sixel_encoder_setopt(encoder[mode], '~', "none") != SIXEL_OK ||
                sixel_encoder_setopt(encoder[mode], '.', "8bit") != SIXEL_OK ||
                sixel_encoder_setopt(encoder[mode], 'p', "16") != SIXEL_OK ||
                sixel_encoder_setopt(encoder[mode], 't', hls ? "hls" : "rgb") !=
                    SIXEL_OK ||
                sixel_encoder_setopt(encoder[mode], '=', "greedy:B1") !=
                    SIXEL_OK ||
                sixel_encoder_setopt(encoder[mode], 'Z',
                    (hls || strcmp(dither, "fs") == 0) ? "delta:T8"
                        : "delta:T8:refresh_ms=80") != SIXEL_OK) goto end;
    }
    for (sequence = 0; sequence < 2; ++sequence) {
        for (mode = 0; mode < 2; ++mode) {
            for (i = 0; i < (int)sizeof(pixels); ++i) {
                pixels[i] = (unsigned char)((i * 53 + sequence * 3) % 251);
            }
            encoder[mode]->refresh_time_override = 1;
            encoder[mode]->refresh_time_us = (uint64_t)sequence * 16667;
            if (sixel_frame_new(&frame, NULL) != SIXEL_OK) goto end;
            status = sixel_frame_init_borrowed(frame, pixels, 16, 16,
                SIXEL_PIXELFORMAT_RGB888, NULL, -1);
            if (status != SIXEL_OK) goto end;
            status = sixel_encoder_encode_frame(encoder[mode], frame,
                output[mode]);
            sixel_frame_unref(frame);
            frame = NULL;
            if (status != SIXEL_OK) goto end;
        }
        /* Observe the path as well as the result: equality through two
         * fallback calls would not exercise the component integration. */
        if (output[0]->indexed_count != (unsigned long)(sequence + 1) ||
                output[1]->indexed_count != 0) goto end;
        if (history) {
            if (encoder[0]->accumulation_pixels_size != sizeof(pixels) ||
                    encoder[1]->accumulation_pixels_size != sizeof(pixels) ||
                    memcmp(encoder[0]->accumulation_pixels,
                        encoder[1]->accumulation_pixels, sizeof(pixels)) ||
                    memcmp(encoder[0]->accumulation_valid_mask,
                        encoder[1]->accumulation_valid_mask, 16 * 16)) goto end;
        } else if (stream[0].size != stream[1].size ||
                memcmp(stream[0].data, stream[1].data, stream[0].size)) {
            goto end;
        }
    }
    ok = 1;
end:
    if (!ok) {
        fprintf(stderr, "indexed equivalence: sequence=%d mode=%d status=%x "
            "native=%lu bytes=%zu/%zu\n", sequence, mode, status,
            output[0] != NULL ? output[0]->indexed_count : 0,
            stream[0].size, stream[1].size);
        for (i = 0; i < (int)stream[0].size && i < (int)stream[1].size; ++i) {
            if (stream[0].data[i] != stream[1].data[i]) {
                fprintf(stderr, "first byte difference %d: %u/%u\n", i,
                    stream[0].data[i], stream[1].data[i]);
                break;
            }
        }
    }
    sixel_frame_unref(frame);
    for (mode = 0; mode < 2; ++mode) {
        sixel_output_unref(output[mode]);
        sixel_encoder_unref(encoder[mode]);
    }
    return ok ? 0 : 1;
}
