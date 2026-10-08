/* SPDX-License-Identifier: MIT */
/* Compare the rendered first APNG frame, including transparent pixels.
 * Independent bands may reselect an already active register; redundant
 * selections are not differences in threaded dither output. */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
#include <string.h>
#include <sixel.h>

int
test_interframe_equal_pixels(int argc, char **argv)
{
    sixel_allocator_t *allocator;
    sixel_decode_options_t options;
    sixel_decode_result_t decoded[2];
    int side;
    int row;
    int ok;

    allocator = NULL;
    ok = 0;
    memset(&options, 0, sizeof(options));
    memset(decoded, 0, sizeof(decoded));
    options.preferred_pixelformat = SIXEL_PIXELFORMAT_RGBA8888;
    if (argc != 3 || SIXEL_FAILED(sixel_allocator_new(&allocator,
            NULL, NULL, NULL, NULL))) {
        goto end;
    }
    for (side = 0; side < 2; ++side) {
        if (SIXEL_FAILED(sixel_decode_pixels(
                (unsigned char const *)argv[side + 1], strlen(argv[side + 1]),
                &options, decoded + side, allocator)) ||
                decoded[side].width != 12 || decoded[side].height != 8 ||
                decoded[side].pixelformat != SIXEL_PIXELFORMAT_RGBA8888 ||
                decoded[side].pixels == NULL || decoded[side].stride < 48) {
            goto end;
        }
    }
    for (row = 0; row < 8; ++row) {
        if (memcmp(decoded[0].pixels + row * decoded[0].stride,
                   decoded[1].pixels + row * decoded[1].stride, 48) != 0) {
            goto end;
        }
    }
    ok = 1;
end:
    if (allocator != NULL) {
        sixel_allocator_free(allocator, decoded[0].pixels);
        sixel_allocator_free(allocator, decoded[1].pixels);
    }
    sixel_allocator_unref(allocator);
    return ok ? 0 : 1;
}
