/*
 * SPDX-License-Identifier: MIT
 *
 * Resolve absolute rows before trusted-raster clipping and flag updates.
 * Assert literal pixels and masks after a successful parallel request, so a
 * serial fallback cannot hide a broken boundary-state reconstruction.
 */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sixel.h>

#include "src/decoder-image.h"
#include "src/decoder-parallel.h"

int
test_decoder_0092(int argc, char **argv)
{
    unsigned char payload[] = "!8~-!8~-!8~-!8~-!8~-!8~-!8~-!8~";
    char const *budgets[] = { "2", "4" };
    sixel_allocator_t *allocator;
    image_buffer_t *image;
    SIXELSTATUS status;
    unsigned char *pixel;
    int outside;
    int expected;
    int depth;
    int budget;
    int y;
    int x;
    int ok;

    (void)argc;
    (void)argv;
    allocator = NULL;
    image = NULL;
    ok = 0;
    status = sixel_allocator_new(&allocator, NULL, NULL, NULL, NULL);
    if (SIXEL_FAILED(status)) {
        return EXIT_FAILURE;
    }
    image = (image_buffer_t *)calloc(1u, sizeof(*image));
    if (image == NULL) {
        goto end;
    }
    for (depth = 1; depth <= 4; depth *= 2) {
        for (budget = 0; budget < 2; ++budget) {
            status = image_buffer_init(image, 8, 7, 0, depth, 1,
                                       allocator);
            if (SIXEL_FAILED(status)) {
                goto end;
            }
            image->palette[2] = 0xff0000;
            image->palette[3] = 0x00ff00;
            status = sixel_decoder_parallel_override_threads(budgets[budget]);
            if (SIXEL_FAILED(status)) {
                goto end;
            }
            outside = 0;
            status = sixel_decoder_parallel_request_start(
                depth == 4, 0, payload, (int)sizeof(payload) - 1,
                payload, image, 2, image->palette, NULL,
                SIXEL_DECODE_PIXELS_OPTION_TRUST_RASTER_SIZE, &outside, NULL);
            if (status != SIXEL_OK || outside != 1) {
                fprintf(stderr, "parallel status=%04x outside=%d\n",
                        status, outside);
                goto end;
            }
            for (y = 0; y < 7; ++y) {
                expected = 2;
                for (x = 0; x < 8; ++x) {
                    pixel = image->pixels.in_bytes + (y * 8 + x) * depth;
                    if (image->paint_mask[y * 8 + x] != 255 ||
                            (depth == 1 && pixel[0] != expected) ||
                            (depth == 2 &&
                             image->pixels.in_shorts[y * 8 + x] != expected) ||
                            (depth == 4 &&
                             (pixel[0] != (expected == 2 ? 255 : 0) ||
                              pixel[1] != (expected == 3 ? 255 : 0) ||
                              pixel[2] != 0 || pixel[3] != 255))) {
                        fprintf(stderr, "pixel mismatch at %d,%d depth=%d\n",
                                x, y, depth);
                        goto end;
                    }
                }
            }
            sixel_allocator_free(allocator, image->paint_mask);
            sixel_allocator_free(allocator, image->pixels.p);
            memset(image, 0, sizeof(*image));
        }
    }
    ok = 1;
end:
    (void)sixel_decoder_parallel_override_threads("1");
    if (image != NULL) {
        sixel_allocator_free(allocator, image->paint_mask);
        sixel_allocator_free(allocator, image->pixels.p);
        free(image);
    }
    sixel_allocator_unref(allocator);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
