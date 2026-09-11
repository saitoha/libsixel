/* SPDX-License-Identifier: MIT */
/* Decode OR then normal with the same decoder without leaking OR state. */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
#include <stdlib.h>
#include <string.h>
#include <sixel.h>
#include "ormode_test_common.h"

int
test_or_dec_0039(int argc, char **argv)
{
    sixel_decoder_t *decoder;
    sixel_allocator_t *allocator;
    sixel_decode_options_t options;
    sixel_decode_result_t result;
    unsigned char payload[] =
        "\033P7;5q\"1;1;1;1#1;2;100;0;0#2;2;0;100;0"
        "#3;2;0;0;100#1@$#2@\033\\";
    int ok;

    (void)argc;
    (void)argv;
    decoder = NULL;
    allocator = NULL;
    ok = 0;
    memset(&options, 0, sizeof(options));
    memset(&result, 0, sizeof(result));
    options.preferred_pixelformat = SIXEL_PIXELFORMAT_RGBA8888;
    if (SIXEL_FAILED(sixel_allocator_new(&allocator, NULL, NULL,
                                         NULL, NULL)) ||
            SIXEL_FAILED(sixel_decoder_new(&decoder, allocator))) {
        goto end;
    }
    if (SIXEL_FAILED(sixel_decoder_decode_pixels(decoder, payload,
            sizeof(payload) - 1U, &options, &result))) {
        goto end;
    }
    if (result.width != 1 || result.height != 1 ||
            memcmp(result.pixels, "\0\0\377\377", 4) != 0) {
        goto end;
    }
    sixel_allocator_free(allocator, result.pixels);
    memset(&result, 0, sizeof(result));
    /* Change only P2, keeping the same overlapping paint commands. */
    payload[4] = '1';
    if (SIXEL_FAILED(sixel_decoder_decode_pixels(decoder, payload,
            sizeof(payload) - 1U, &options, &result))) {
        goto end;
    }
    ok = result.width == 1 && result.height == 1 &&
        memcmp(result.pixels, "\0\377\0\377", 4) == 0;
end:
    if (allocator != NULL) {
        sixel_allocator_free(allocator, result.pixels);
    }
    if (decoder != NULL) {
        sixel_decoder_unref(decoder);
    }
    if (allocator != NULL) {
        sixel_allocator_unref(allocator);
    }
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
