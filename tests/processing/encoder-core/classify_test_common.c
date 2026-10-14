/* SPDX-License-Identifier: MIT */
/* Frozen bodies distinguish row order, byte masks, and size-policy holes. */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
#include <stdio.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <sixel.h>
#include "src/encoder-core-private.h"
#include "src/threading.h"

SIXELSTATUS sixel_encode_body(sixel_index_t *, int, int, unsigned char *,
    float const *, int, int, int, sixel_output_t *, unsigned char *,
    sixel_allocator_t *, int, sixel_timeline_logger_t *);

static int
classify_test_discard(char *data, int size, void *opaque)
{
    (void)data;
    (void)opaque;
    return size;
}

int
test_classify_stream(int scenario, char const *expected)
{
    static unsigned char const colors[] = { 0, 1, 2, 3, 4, 255 };
    sixel_index_t pixels[36];
    unsigned char mask[36];
    unsigned char palstate[2];
    sixel_allocator_t *allocator;
    sixel_output_t *output;
    SIXELSTATUS status;
    int width;
    int height;
    int ncolors;
    int keycolor;
    int x;
    int y;
    int saved_threads;
    int ok;

    allocator = NULL;
    output = NULL;
    ok = 0;
    saved_threads = sixel_threads_resolve();
    sixel_set_threads(1);
    memset(mask, 0, sizeof(mask));
    memset(palstate, 1, sizeof(palstate));
    width = 2;
    height = 3;
    ncolors = 3;
    keycolor = 2;
    if (scenario == 0) {
        /* A precedes B by rows, but B precedes A by columns. Both nodes
         * span [0, 2), so the later-inserted color must be emitted first. */
        pixels[0] = 2;
        pixels[1] = 0;
        pixels[2] = 1;
        pixels[3] = 1;
        pixels[4] = 0;
        pixels[5] = 2;
    } else if (scenario == 1) {
        width = 6;
        height = 6;
        ncolors = 4;
        keycolor = 3;
        /* Repeated painted rows must merge, while the key and out-of-range
         * bytes must not turn into color-zero occurrences in empty rows. */
        for (y = 0; y < height; y++) {
            for (x = 0; x < width; x++) {
                pixels[y * width + x] = colors[(x + y / 2) % 6];
            }
        }
    } else {
        width = 4;
        height = 2;
        ncolors = 2;
        keycolor = -1;
        for (y = 0; y < height; y++) {
            for (x = 0; x < width; x++) {
                pixels[y * width + x] = (sixel_index_t)((x + y) % 2);
            }
        }
        /* A mask always disables fill, even with palstate and a valid
         * first source pixel. Otherwise the first node paints this hole. */
        mask[0] = 1;
    }
    status = sixel_allocator_new(&allocator, NULL, NULL, NULL, NULL);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    status = sixel_output_new(&output, classify_test_discard, NULL, allocator);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    output->encode_policy = scenario == 2 ?
        SIXEL_ENCODEPOLICY_SIZE : SIXEL_ENCODEPOLICY_FAST;
    output->transparent_mask = scenario == 2 ? mask : NULL;
    status = sixel_encode_body(pixels, width, height, NULL, NULL, ncolors,
        keycolor, 1, output, scenario == 2 ? palstate : NULL, allocator,
        0, NULL);
    if (SIXEL_FAILED(status) || output->pos < 0 ||
            output->pos >= SIXEL_OUTPUT_PACKET_SIZE) {
        goto end;
    }
    output->buffer[output->pos] = '\0';
    if (strcmp((char *)output->buffer, expected) != 0) {
        fprintf(stderr, "classify case %d: [%s], expected [%s]\n",
            scenario, output->buffer, expected);
        goto end;
    }
    ok = 1;
end:
    sixel_output_unref(output);
    sixel_allocator_unref(allocator);
    sixel_set_threads(saved_threads);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}

int
test_classify_bounds(int color_plane)
{
    sixel_index_t pixels[2] = { 0, 0 };
    sixel_allocator_t *allocator;
    sixel_output_t *output;
    SIXELSTATUS status;
    int width;
    int height;
    int saved_threads;
    int ok;

    allocator = NULL;
    output = NULL;
    ok = 0;
    saved_threads = sixel_threads_resolve();
    sixel_set_threads(1);
    width = color_plane ? INT_MAX / 256 + 1 : 2;
    height = color_plane ? 1 : INT_MAX / 2 + 1;
    status = sixel_allocator_new(&allocator, NULL, NULL, NULL, NULL);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    status = sixel_output_new(&output, classify_test_discard, NULL, allocator);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    /* Only the dimensions are huge. Reject them before any source read or
     * scratch allocation; a two-byte buffer is sufficient for this call. */
    status = sixel_encode_body(pixels, width, height, NULL, NULL, 256,
        -1, 1, output, NULL, allocator, 0, NULL);
    ok = status == SIXEL_BAD_INTEGER_OVERFLOW && output->pos == 0;
end:
    sixel_output_unref(output);
    sixel_allocator_unref(allocator);
    sixel_set_threads(saved_threads);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
