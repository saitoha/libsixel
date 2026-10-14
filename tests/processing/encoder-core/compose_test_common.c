/* SPDX-License-Identifier: MIT */
/* Small wire snapshots isolate run gaps and occupancy-word reuse. */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sixel.h>
#include "src/encoder-core-private.h"
#include "src/threading.h"

SIXELSTATUS sixel_encode_body(sixel_index_t *, int, int, unsigned char *,
    float const *, int, int, int, sixel_output_t *, unsigned char *,
    sixel_allocator_t *, int, sixel_timeline_logger_t *);

static int
compose_test_discard(char *data, int size, void *opaque)
{
    (void)data;
    (void)opaque;
    return size;
}

int
test_compose_stream(int scenario, char const *expected)
{
    sixel_index_t pixels[130 * 13];
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
    width = 12;
    height = 1;
    saved_threads = sixel_threads_resolve();
    sixel_set_threads(1);
    memset(pixels, 2, sizeof(pixels));
    if (scenario < 2) {
        /* The middle color makes a changed node boundary observable:
         * a joined run must return left to emit it, a split run need not. */
        pixels[0] = 0;
        pixels[5] = 1;
        pixels[10 + scenario] = 0;
    } else if (scenario == 2) {
        width = 257;
        pixels[0] = 0;
        pixels[63] = 0;
        pixels[64] = 0;
        pixels[127] = 1;
        pixels[255] = 0;
        pixels[256] = 0;
    } else {
        /* Color zero disappears for a band before returning in a new word.
         * Its old occupied column must not survive in the reusable bitmap. */
        width = 130;
        height = 13;
        pixels[0] = 0;
        pixels[6 * width + 64] = 1;
        pixels[12 * width + 129] = 0;
    }
    status = sixel_allocator_new(&allocator, NULL, NULL, NULL, NULL);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    status = sixel_output_new(&output, compose_test_discard, NULL, allocator);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    output->encode_policy = SIXEL_ENCODEPOLICY_FAST;
    status = sixel_encode_body(pixels, width, height, NULL, NULL, 3, 2, 1,
        output, NULL, allocator, 0, NULL);
    if (SIXEL_FAILED(status) || output->pos < 0 ||
            output->pos >= SIXEL_OUTPUT_PACKET_SIZE) {
        goto end;
    }
    output->buffer[output->pos] = '\0';
    if (strcmp((char *)output->buffer, expected) != 0) {
        fprintf(stderr, "compose case %d: [%s], expected [%s]\n",
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
