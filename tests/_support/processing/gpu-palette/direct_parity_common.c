/* SPDX-License-Identifier: MIT */
/* Exact CPU parity for the direct (lookup NONE) GPU palette apply. Every GPU
 * backend that claims this stage, including a partial one, must emit the
 * same SIXEL bytes as the CPU for the same palette, method and image. */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sixel.h>
#include "src/dither.h"
#include "src/output.h"

#define DIRECT_PARITY_WIDTH 64
#define DIRECT_PARITY_HEIGHT 48

typedef struct direct_parity_stream {
    unsigned char *bytes;
    size_t size;
    size_t capacity;
} direct_parity_stream_t;

static int
direct_parity_write(char *bytes, int size, void *context)
{
    direct_parity_stream_t *stream;
    unsigned char *grown;
    size_t capacity;

    stream = context;
    if (size < 0) return -1;
    if ((size_t)size > stream->capacity - stream->size) {
        capacity = stream->capacity * 2U + (size_t)size;
        grown = realloc(stream->bytes, capacity);
        if (grown == NULL) return -1;
        stream->bytes = grown;
        stream->capacity = capacity;
    }
    memcpy(stream->bytes + stream->size, bytes, (size_t)size);
    stream->size += (size_t)size;
    return size;
}

static unsigned int
direct_parity_next(unsigned int *state)
{
    *state = *state * 1103515245U + 12345U;
    return (*state >> 16) & 0x7fffU;
}

static int
direct_parity_encode(unsigned char *pixels, unsigned char const *palette,
    int ncolors, int method, int policy, direct_parity_stream_t *stream)
{
    sixel_dither_t *dither;
    sixel_output_t *output;
    SIXELSTATUS status;

    dither = NULL;
    output = NULL;
    status = sixel_dither_new(&dither, ncolors, NULL);
    if (SIXEL_SUCCEEDED(status)) {
        status = sixel_output_new(&output, direct_parity_write, stream,
            NULL);
    }
    if (SIXEL_SUCCEEDED(status)) {
        sixel_dither_set_palette(dither, (unsigned char *)palette);
        dither->force_palette = 1;
        dither->bluenoise_strength_override = 1;
        dither->bluenoise_strength = 0.5f;
        dither->a_dither_strength_override = 1;
        dither->x_dither_strength_override = 1;
        dither->a_dither_strength = 0.5f;
        dither->x_dither_strength = 0.5f;
        sixel_dither_set_lut_policy(dither, SIXEL_LUT_POLICY_NONE);
        sixel_dither_set_diffusion_type(dither, method);
        /* FORCE reports errors instead of silently falling back to CPU. */
        dither->gpu_policy = policy;
        status = sixel_encode(pixels, DIRECT_PARITY_WIDTH,
            DIRECT_PARITY_HEIGHT, 3, dither, output);
    }
    if (SIXEL_FAILED(status)) {
        fprintf(stderr, "direct parity encode (policy %d): %s\n", policy,
            sixel_helper_get_additional_message());
    }
    sixel_output_unref(output);
    sixel_dither_unref(dither);
    return SIXEL_SUCCEEDED(status) ? 0 : 1;
}

int
test_gpu_direct_parity(int method, int ncolors)
{
    unsigned char pixels[DIRECT_PARITY_WIDTH * DIRECT_PARITY_HEIGHT * 3];
    unsigned char palette[SIXEL_PALETTE_MAX * 3];
    direct_parity_stream_t streams[2];
    unsigned int state;
    int x, y, i, pass, ok;
    unsigned char *pixel;

    /* Smooth gradients exercise positional noise; the LCG term adds ties
     * and near-ties so index order in the scan is observable. */
    state = 0x5eed1234U + (unsigned int)method * 97U +
        (unsigned int)ncolors;
    for (y = 0; y < DIRECT_PARITY_HEIGHT; ++y) {
        for (x = 0; x < DIRECT_PARITY_WIDTH; ++x) {
            pixel = pixels + ((size_t)y * DIRECT_PARITY_WIDTH + x) * 3U;
            pixel[0] = (unsigned char)(x * 4 + direct_parity_next(&state) % 5);
            pixel[1] = (unsigned char)(y * 5 + direct_parity_next(&state) % 5);
            pixel[2] = (unsigned char)((x + y) * 2 +
                direct_parity_next(&state) % 5);
        }
    }
    for (i = 0; i < ncolors * 3; ++i) {
        palette[i] = (unsigned char)(direct_parity_next(&state) & 0xffU);
    }
    memset(streams, 0, sizeof(streams));
    ok = 0;
    for (pass = 0; pass < 2; ++pass) {
        if (direct_parity_encode(pixels, palette, ncolors, method,
                pass ? SIXEL_GPU_POLICY_FORCE : SIXEL_GPU_POLICY_OFF,
                &streams[pass])) goto end;
    }
    if (streams[0].size == 0 || streams[0].size != streams[1].size ||
            memcmp(streams[0].bytes, streams[1].bytes, streams[0].size)) {
        fprintf(stderr, "direct parity mismatch method=%d colors=%d "
            "cpu=%zu gpu=%zu bytes\n", method, ncolors, streams[0].size,
            streams[1].size);
        goto end;
    }
    ok = 1;
end:
    free(streams[0].bytes);
    free(streams[1].bytes);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
