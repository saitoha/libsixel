/*
 * SPDX-License-Identifier: MIT
 *
 * A tiny trusted canvas can have a large encoded paint extent. Verify that
 * parallel validation and publication handle it without signed overflow.
 * Sanitizer builds also check the diagnostic visit counter during SCAN.
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
test_decoder_0093(int argc, char **argv)
{
    sixel_allocator_t *allocator;
    image_buffer_t *image;
    unsigned char *payload;
    SIXELSTATUS status;
    int band;
    int x;
    int outside;
    int ok;

    (void)argc;
    (void)argv;
    allocator = NULL;
    image = NULL;
    payload = NULL;
    ok = 0;
    status = sixel_allocator_new(&allocator, NULL, NULL, NULL, NULL);
    if (SIXEL_FAILED(status)) {
        return EXIT_FAILURE;
    }
    image = (image_buffer_t *)calloc(1u, sizeof(*image));
    payload = (unsigned char *)malloc(96000u);
    if (image == NULL || payload == NULL) {
        goto end;
    }
    /* Two workers each visit 6000 bands of 65535 fully set columns. The
     * retained canvas is only one row, so allocation stays small. */
    for (band = 0; band < 12000; ++band) {
        memcpy(payload + band * 8, "!65535~-", 8u);
    }
    status = image_buffer_init(image, 65535, 1, 0, 1, 1, allocator);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    status = sixel_decoder_parallel_override_threads("2");
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    outside = 0;
    status = sixel_decoder_parallel_request_start(
        0, 0, payload, 96000, payload, image, 2, image->palette, NULL,
        SIXEL_DECODE_PIXELS_OPTION_TRUST_RASTER_SIZE, &outside, NULL);
    if (status != SIXEL_OK || outside != 1) {
        fprintf(stderr, "clipped scan status=%04x outside=%d\n",
                status, outside);
        goto end;
    }
    for (x = 0; x < 65535; ++x) {
        if (image->pixels.in_bytes[x] != 2 || image->paint_mask[x] != 255) {
            fprintf(stderr, "clipped scan pixel mismatch at %d\n", x);
            goto end;
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
    free(payload);
    sixel_allocator_unref(allocator);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
