/* SPDX-License-Identifier: MIT */
/* Frozen DECGRI bytes, including the packet callback and retained suffix. */
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

struct emit_run_sink {
    char const *expected;
    size_t length;
    size_t written;
    int packets;
    int ok;
};

static int
emit_run_write(char *data, int size, void *opaque)
{
    struct emit_run_sink *sink;

    sink = (struct emit_run_sink *)opaque;
    if (size != SIXEL_OUTPUT_PACKET_SIZE ||
            (size_t)size > sink->length - sink->written ||
            memcmp(data, sink->expected + sink->written, (size_t)size)) {
        sink->ok = 0;
        return -1;
    }
    sink->written += (size_t)size;
    sink->packets++;
    return size;
}

int
test_emit_run_stream(int limited, int packet)
{
    static int const runs[] = {3, 4, 9, 10, 99, 100, 255, 256, 999, 1000};
    /* These literal spellings are independent of the production formatter.
     * 256 splits as 255 + one literal; 999/1000 as 3 * 255 + 234/235. */
    static char const unlimited[] =
        "@@@!4A!9@!10A!99@!100A!255@!256A!999@!1000A";
    static char const constrained[] =
        "@@@!4A!9@!10A!99@!100A!255@!255AA"
        "!255@!255@!255@!234@!255A!255A!255A!235A";
    struct emit_run_sink sink;
    sixel_allocator_t *allocator;
    sixel_output_t *output;
    sixel_index_t *pixels;
    char *expected;
    char const *body;
    SIXELSTATUS status;
    size_t body_length;
    int saved_threads;
    int alignment;
    int prefix;
    int width;
    int x;
    int row;
    int run;
    int mask;
    int end;
    int ok;

    allocator = NULL;
    output = NULL;
    pixels = NULL;
    expected = NULL;
    body = limited ? constrained : unlimited;
    body_length = strlen(body);
    ok = 0;
    saved_threads = sixel_threads_resolve();
    sixel_set_threads(1);
    status = sixel_allocator_new(&allocator, NULL, NULL, NULL, NULL);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    for (alignment = 1; alignment <= (packet ? 6 : 1); alignment++) {
        /* Put the final 1000-cell run 1..6 bytes before the packet edge.
         * Its six-byte unlimited spelling and five-byte limited chunks
         * cross at each byte; the literal prefix prevents compression. */
        prefix = packet ? SIXEL_OUTPUT_PACKET_SIZE - 3 -
            ((int)body_length - (limited ? 20 : 6)) - alignment : 0;
        width = 4 + prefix;
        for (run = 0; run < 10; run++) {
            width += runs[run];
        }
        pixels = calloc((size_t)width * 6u, sizeof(*pixels));
        expected = malloc(3u + (size_t)prefix + body_length);
        if (pixels == NULL || expected == NULL) {
            goto end;
        }
        memcpy(expected, "!4?", 3u);
        x = 4;
        for (end = 4 + prefix; x < end; x++) {
            mask = (x - 4) % 2 + 2;
            expected[3 + x - 4] = (char)('?' + mask);
            for (row = 0; row < 6; row++) {
                pixels[row * width + x] = (sixel_index_t)(
                    (mask >> row) & 1);
            }
        }
        memcpy(expected + 3 + prefix, body, body_length);
        for (run = 0; run < 10; run++) {
            mask = run % 2 + 1;
            for (end = x + runs[run]; x < end; x++) {
                for (row = 0; row < 6; row++) {
                    pixels[row * width + x] = (sixel_index_t)(
                        (mask >> row) & 1);
                }
            }
        }
        sink.expected = expected;
        sink.length = 3u + (size_t)prefix + body_length;
        sink.written = 0u;
        sink.packets = 0;
        sink.ok = 1;
        status = sixel_output_new(&output, emit_run_write, &sink, allocator);
        if (SIXEL_FAILED(status)) {
            goto end;
        }
        output->encode_policy = SIXEL_ENCODEPOLICY_FAST;
        output->has_gri_arg_limit = limited;
        status = sixel_encode_body(pixels, width, 6, NULL, NULL, 2, 0, 1,
            output, NULL, allocator, 0, NULL);
        if (SIXEL_FAILED(status) || !sink.ok ||
                sink.packets != packet || output->pos < 0 ||
                (size_t)output->pos != sink.length - sink.written ||
                output->save_count != 0 ||
                memcmp(output->buffer, expected + sink.written,
                       (size_t)output->pos)) {
            fprintf(stderr, "DECGRI limit=%d packet=%d alignment=%d: "
                "status=%x packets=%d pos=%d expected tail=%lu\n",
                limited, packet, alignment, status, sink.packets,
                output->pos, (unsigned long)(sink.length - sink.written));
            goto end;
        }
        sixel_output_unref(output);
        output = NULL;
        free(pixels);
        pixels = NULL;
        free(expected);
        expected = NULL;
    }
    ok = 1;
end:
    sixel_output_unref(output);
    sixel_allocator_unref(allocator);
    free(pixels);
    free(expected);
    sixel_set_threads(saved_threads);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
