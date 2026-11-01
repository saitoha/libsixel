/* SPDX-License-Identifier: MIT */
/* Frozen wire hashes protect node order, including size-policy overpainting. */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sixel.h>
#include "src/encoder-core-private.h"
#include "src/threading.h"

/* These internal entry points also serve the high-color encoder. Calling
 * the body directly exposes palstate and an independent transparency mask. */
SIXELSTATUS sixel_encode_header(int, int, int, sixel_output_t *);
SIXELSTATUS sixel_encode_body(sixel_index_t *, int, int, unsigned char *,
    float const *, int, int, int, sixel_output_t *, unsigned char *,
    sixel_allocator_t *, int, sixel_timeline_logger_t *);
SIXELSTATUS sixel_encode_footer(sixel_output_t *);

typedef struct node_unlink_hash {
    uint32_t value;
    size_t bytes;
} node_unlink_hash_t;

static int
node_unlink_write(char *data, int size, void *opaque)
{
    node_unlink_hash_t *hash;
    int i;

    hash = opaque;
    /* FNV-1a with explicit reduction avoids unsigned-overflow sanitizer
     * noise while retaining the same platform-independent 32-bit hash. */
    for (i = 0; i < size; ++i) {
        hash->value = (uint32_t)(((uint64_t)(hash->value ^
            (unsigned char)data[i]) * UINT64_C(16777619)) &
            UINT64_C(0xffffffff));
    }
    hash->bytes += (size_t)size;
    return size;
}

int
test_node_unlink_stream(int policy, unsigned long serial,
                        unsigned long parallel)
{
    static int const tops[] = { 0, 1, 5, 6, 7 };
    sixel_index_t pixels[97 * 73];
    unsigned char mask[97 * 73];
    unsigned char palette[32 * 3];
    unsigned char palstate[32];
    sixel_allocator_t *allocator;
    sixel_output_t *output;
    node_unlink_hash_t hash;
    SIXELSTATUS status;
    unsigned long expected;
    int saved_threads;
    int threads;
    int max_threads;
    int key;
    int masked;
    int stateful;
    int offset;
    int x;
    int y;
    int i;
    int ok;

    allocator = NULL;
    output = NULL;
    ok = 0;
    saved_threads = sixel_threads_resolve();
#if SIXEL_ENABLE_THREADS
    max_threads = 12;
#else
    max_threads = 1;
#endif
    status = sixel_allocator_new(&allocator, NULL, NULL, NULL, NULL);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    for (i = 0; i < 32; ++i) {
        palette[3 * i] = (unsigned char)(i * 7);
        palette[3 * i + 1] = (unsigned char)(255 - i * 5);
        palette[3 * i + 2] = (unsigned char)(i * 3);
        palstate[i] = 1;
    }
    for (threads = 1; threads <= max_threads; ++threads) {
        sixel_set_encoder_threads(threads);
        hash.value = UINT32_C(2166136261);
        hash.bytes = 0;
        for (key = 0; key < 2; ++key) {
            for (masked = 0; masked < 2; ++masked) {
                for (stateful = 0; stateful < 2; ++stateful) {
                    for (offset = 0; offset < 5; ++offset) {
                        /* Long gaps split a color into multiple nodes.
                         * Overlapping runs force skipped predecessors;
                         * 13 bands exercise worker reuse and a tail row. */
                        for (y = 0; y < 73; ++y) {
                            for (x = 0; x < 97; ++x) {
                                i = y * 97 + x;
                                pixels[i] = (sixel_index_t)((x / 3 +
                                    y * 7 + (x / 29) * 11) % 32);
                                /* The high-color sentinel disables fill
                                 * when it leads a palstate source band. */
                                if (stateful && (x + y * 3) % 19 == 0) {
                                    pixels[i] = 32;
                                }
                                mask[i] = (unsigned char)(
                                    (y >= 24 && y < 30) ||
                                    (y % 12 < 6 && (x + y) % 7 == 0));
                            }
                        }
                        status = sixel_output_new(&output,
                            node_unlink_write, &hash, allocator);
                        if (SIXEL_FAILED(status)) {
                            goto end;
                        }
                        output->encode_policy = policy;
                        output->transparent_mask = masked ? mask : NULL;
                        output->transparent_offset_left = offset ? 3 : 0;
                        output->transparent_offset_top = tops[offset];
                        status = sixel_encode_header(
                            97 + output->transparent_offset_left,
                            73 + tops[offset], key ? 7 : -1, output);
                        if (SIXEL_FAILED(status)) {
                            goto end;
                        }
                        status = sixel_encode_body(pixels, 97, 73,
                            palette, NULL, 32, key ? 7 : -1, 0, output,
                            stateful ? palstate : NULL, allocator, 0, NULL);
                        if (SIXEL_FAILED(status)) {
                            goto end;
                        }
                        status = sixel_encode_footer(output);
                        if (SIXEL_FAILED(status)) {
                            goto end;
                        }
                        sixel_output_unref(output);
                        output = NULL;
                    }
                }
            }
        }
        /* Serial output retains the active palette across bands; workers
         * reset it per band. Compare each route with its own frozen bytes,
         * not with another path through the implementation under test. */
        expected = threads == 1 ? serial : parallel;
        if (hash.bytes == 0 || (unsigned long)hash.value != expected) {
            fprintf(stderr, "node order policy=%d threads=%d: "
                "%08lx (%lu bytes), expected %08lx\n", policy, threads,
                (unsigned long)hash.value, (unsigned long)hash.bytes,
                expected);
            goto end;
        }
    }
    ok = 1;
end:
    sixel_output_unref(output);
    sixel_allocator_unref(allocator);
    sixel_set_encoder_threads(saved_threads);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
