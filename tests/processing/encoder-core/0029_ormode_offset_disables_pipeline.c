/* SPDX-License-Identifier: MIT */
/* OR offsets must select the serial body even with a multi-worker budget. */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sixel.h>
#include "src/compat_stub.h"
#include "src/dither.h"
#include "src/encoder-core-private.h"
#include "src/threading.h"

static int
or_offset_write(char *data, int size, void *opaque)
{
    char *buffer;
    size_t used;

    buffer = (char *)opaque;
    used = strlen(buffer);
    if (size < 0 || used + (size_t)size >= 4096U) {
        return -1;
    }
    memcpy(buffer + used, data, (size_t)size);
    buffer[used + (size_t)size] = '\0';
    return size;
}

int
test_or_enc_0029(int argc, char **argv)
{
    unsigned char palette[12] = {
        0, 0, 0, 255, 0, 0, 0, 255, 0, 0, 0, 255
    };
    unsigned char source[4 * 13 * 3];
    char stream[4096];
    sixel_dither_t *dither;
    sixel_output_t *output;
    sixel_allocator_t *allocator;
    unsigned char *decoded;
    int width;
    int height;
    int x;
    int y;
    int expected;
    int ok;

    (void)argc;
    (void)argv;
    dither = NULL;
    output = NULL;
    allocator = NULL;
    decoded = NULL;
    stream[0] = '\0';
    ok = 0;
    if (sixel_compat_setenv("SIXEL_THREADS", "2") ||
            sixel_threads_resolve() != 2) {
        goto end;
    }
    for (y = 0; y < 13; y++) {
        for (x = 0; x < 4; x++) {
            memcpy(source + (y * 4 + x) * 3,
                   palette + ((x + y) & 3) * 3, 3);
        }
    }
    if (SIXEL_FAILED(sixel_allocator_new(&allocator, NULL, NULL,
                                         NULL, NULL)) ||
            SIXEL_FAILED(sixel_dither_new(&dither, 4, allocator)) ||
            SIXEL_FAILED(sixel_output_new(&output, or_offset_write,
                                           stream, allocator))) {
        goto end;
    }
    sixel_dither_set_palette(dither, palette);
    sixel_dither_set_pixelformat(dither, SIXEL_PIXELFORMAT_RGB888);
    sixel_dither_set_diffusion_type(dither, SIXEL_DIFFUSE_NONE);
    sixel_dither_set_optimize_palette(dither, 0);
    dither->force_palette = 1;
    sixel_output_set_ormode(output, 1);
    output->transparent_policy = SIXEL_ALPHA_POLICY_KEEP;
    output->transparent_offset_left = 3;
    output->transparent_offset_top = 5;
    if (SIXEL_FAILED(sixel_encode(source, 4, 13, 3, dither, output))) {
        goto end;
    }
    if (SIXEL_FAILED(sixel_decode_direct((unsigned char *)stream,
            (int)strlen(stream), &decoded, &width, &height, allocator)) ||
            width != 7 || height != 18) {
        goto end;
    }
    for (y = 0; y < height; y++) {
        for (x = 0; x < width; x++) {
            expected = x < 3 || y < 5 ? 0 : ((x - 3 + y - 5) & 3);
            if (memcmp(decoded + (y * width + x) * 4,
                       palette + expected * 3, 3) != 0 ||
                    decoded[(y * width + x) * 4 + 3] != 255) {
                goto end;
            }
        }
    }
    ok = 1;
end:
    (void)sixel_compat_setenv("SIXEL_THREADS", "1");
    if (output != NULL) {
        sixel_output_unref(output);
    }
    if (dither != NULL) {
        sixel_dither_unref(dither);
    }
    if (allocator != NULL) {
        sixel_allocator_free(allocator, decoded);
        sixel_allocator_unref(allocator);
    }
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
