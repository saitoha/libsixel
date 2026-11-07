/* SPDX-License-Identifier: MIT */
/* Exercise speculative fused allocation and parallel parsing before a late
 * DECGRA. Compare the restarted RGB/RGBA result with row reconstruction of
 * an independently specified device-row indexed image. */
#include <stdlib.h>
#include <string.h>
#include "src/decoder.h"
#include "src/decoder-parallel.h"
#include "src/sixel_decode_pixels.h"

static int restart_fused_allocations;
static int restart_fused_pixel_size;

static void *
restart_fused_malloc(size_t size)
{
    if (size == (size_t)(512 * 144 * restart_fused_pixel_size)) {
        ++restart_fused_allocations;
    }
    return malloc(size);
}

int
test_decoder_aspect_0044(int argc, char **argv)
{
    static char const head[] =
        "\033Pq\"1;1;512;144#1;2;100;0;0#2;2;0;100;0";
    static char const change[] = "\"2;1;512;144";
    static unsigned char palette[] = { 0, 0, 0, 255, 0, 0, 0, 255, 0 };
    unsigned char stream[17000];
    unsigned char indexed[512 * 144];
    unsigned char *p;
    unsigned char *reference;
    unsigned char *pixels;
    sixel_allocator_t *allocator;
    SIXELSTATUS status;
    unsigned int flags;
    int width;
    int height;
    int band;
    int y;
    int row;
    int direct;
    int budget;
    int pos;
    int threads;
    int ok;

    (void)argc;
    (void)argv;
    p = stream;
    memcpy(p, head, sizeof(head) - 1);
    p += sizeof(head) - 1;
    for (band = 0; band < 16; ++band) {
        if (band == 8) {
            memcpy(p, change, sizeof(change) - 1);
            p += sizeof(change) - 1;
        }
        memcpy(p, "#1", 2);
        p += 2;
        memset(p, '~', 512);
        p += 512;
        memcpy(p, "$#2", 3);
        p += 3;
        memset(p, 'T', 512);
        p += 512;
        *p++ = '-';
    }
    memcpy(p, "\033\\", 2);
    p += 2;
    for (y = 0; y < 144; ++y) {
        row = y < 48 ? y : (y - 48) / 2;
        memset(indexed + y * 512, row % 2 == 0 ? 2 : 1, 512);
    }
    allocator = NULL;
    reference = NULL;
    restart_fused_pixel_size = 0;
    status = sixel_allocator_new(&allocator, restart_fused_malloc,
                                 calloc, realloc, free);
    if (status != SIXEL_OK) {
        return EXIT_FAILURE;
    }
    reference = malloc(512U * 144U * 3U);
    ok = reference != NULL;
    if (ok) {
        status = sixel_dequantize_k_undither_fast4_rows(indexed, 512, 144,
            palette, 3, 0, 0, 144, allocator, reference);
        ok = status == SIXEL_OK;
    }
    for (budget = 0; budget < 2 && ok; ++budget) {
        status = sixel_decoder_parallel_override_threads(budget ? "4" : "1");
        threads = sixel_decoder_parallel_resolve_threads();
        ok = status == SIXEL_OK;
        for (direct = 0; direct < 2 && ok; ++direct) {
            pixels = NULL;
            restart_fused_pixel_size = direct ? 4 : 3;
            restart_fused_allocations = 0;
            status = sixel_decode_kundither_fast4_with_options(stream,
                (int)(p - stream), direct, 0, 0U, &flags,
                &pixels, &width, &height, allocator);
            /* One final output allocation plus a discarded fused output
             * when workers are available proves the speculative route. */
            ok = status == SIXEL_OK && width == 512 && height == 144 &&
                flags == 0U &&
                restart_fused_allocations == (threads > 1 ? 2 : 1);
            for (pos = 0; pos < 512 * 144 && ok; ++pos) {
                ok = memcmp(pixels + pos * (direct ? 4 : 3),
                            reference + pos * 3, 3U) == 0;
                if (direct && ok) {
                    ok = pixels[pos * 4 + 3] == 255;
                }
            }
            free(pixels);
        }
    }
    (void)sixel_decoder_parallel_override_threads("1");
    free(reference);
    sixel_allocator_unref(allocator);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
