/* SPDX-License-Identifier: MIT */
/* Frozen size-policy bodies expose stale source bits and underpainted gaps. */
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
clear_map_test_discard(char *data, int size, void *opaque)
{
    (void)data;
    (void)opaque;
    return size;
}

int
test_clear_map_stream(int threads, int ncolors)
{
    static char const dense_expected[] =
        "#0!4~$#1?~@A$#2??}{"
        "-#1!4~$#2?~@A$#0??}{"
        "-#2!4~$#0?~@A$#1??}{"
        "-#0!4~$#1?~@A$#2??}{"
        "-#1!4~$#2?~@A$#0??}{"
        "-#2!4~$#0?~@A$#1??}{"
        "-#0!4~$#1?~@A$#2??}{"
        "-#1!4~$#2?~@A$#0??}{"
        "-#2!4~$#0?~@A$#1??}{";
    static char const sparse_expected[] =
        "#0!4~$#2?~@A$#4??}{"
        "-#2!4~$#4?~@A$#0??}{"
        "-#4!4~$#0?~@A$#2??}{"
        "-#0!4~$#2?~@A$#4??}{"
        "-#2!4~$#4?~@A$#0??}{"
        "-#4!4~$#0?~@A$#2??}{"
        "-#0!4~$#2?~@A$#4??}{"
        "-#2!4~$#4?~@A$#0??}{"
        "-#4!4~$#0?~@A$#2??}{";
    char const *expected;
    sixel_index_t pixels[4 * 54];
    sixel_allocator_t *allocator;
    sixel_output_t *output;
    SIXELSTATUS status;
    int band;
    int row;
    int offset;
    int step;
    int saved_threads;
    int ok;

    /* Five entries leave inactive planes between the three painted colors. */
    expected = ncolors == 5 ? sparse_expected : dense_expected;
    step = ncolors == 5 ? 2 : 1;
    allocator = NULL;
    output = NULL;
    ok = 0;
    saved_threads = sixel_threads_resolve();
    sixel_set_encoder_threads(threads);
    for (band = 0; band < 9; band++) {
        for (row = 0; row < 6; row++) {
            offset = (band * 6 + row) * 4;
            /* A at both ends underpaints [0, 4), including holes 1 and 2.
             * B and C restore those holes; partial masks at columns 2/3
             * must not inherit any of the preceding band's underpaint.
             * Three color rotations with only two workers guarantee that
             * some workspace processes different rotations, regardless of
             * scheduling. Every rotation change exposes stale row bits. */
            pixels[offset] = (sixel_index_t)((band % 3) * step);
            pixels[offset + 1] = (sixel_index_t)(((band + 1) % 3) * step);
            pixels[offset + 2] = (sixel_index_t)(
                ((band + (row == 0 ? 1 : 2)) % 3) * step);
            pixels[offset + 3] = (sixel_index_t)(
                ((band + (row == 0 ? 0 : row == 1 ? 1 : 2)) % 3) * step);
        }
    }
    status = sixel_allocator_new(&allocator, NULL, NULL, NULL, NULL);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    status = sixel_output_new(&output, clear_map_test_discard, NULL, allocator);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    output->encode_policy = SIXEL_ENCODEPOLICY_SIZE;
    status = sixel_encode_body(pixels, 4, 54, NULL, NULL, ncolors, -1, 1,
        output, NULL, allocator, 0, NULL);
    if (SIXEL_FAILED(status) || output->pos < 0 ||
            output->pos >= SIXEL_OUTPUT_PACKET_SIZE) {
        goto end;
    }
    output->buffer[output->pos] = '\0';
    if (strcmp((char *)output->buffer, expected) != 0) {
        fprintf(stderr, "clear map (%d threads): [%s], expected [%s]\n",
            threads, output->buffer, expected);
        goto end;
    }
    ok = 1;
end:
    sixel_output_unref(output);
    sixel_allocator_unref(allocator);
    sixel_set_encoder_threads(saved_threads);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
