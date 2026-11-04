/* SPDX-License-Identifier: MIT */
/* Compare the wire selector, including bytes flushed at a packet boundary. */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
#include <stdio.h>
#include <string.h>
#include <sixel.h>
#include "src/encoder-core-private.h"
#include "src/threading.h"

struct cpu_register_sink {
    char const *body;
    size_t prefix;
    size_t position;
    size_t length;
    int ok;
};

static int
cpu_register_write(char *data, int size, void *opaque)
{
    struct cpu_register_sink *sink;
    size_t position;
    int i;
    char expected;

    sink = (struct cpu_register_sink *)opaque;
    for (i = 0; i < size; i++) {
        position = sink->position++;
        if (position >= sink->prefix + sink->length) {
            sink->ok = 0;
            continue;
        }
        expected = position < sink->prefix ? '?' :
            sink->body[position - sink->prefix];
        if (data[i] != expected) sink->ok = 0;
    }
    return size;
}

int
test_cpu_register(unsigned int reg, int packet, char const *expected)
{
    struct cpu_register_sink sink;
    sixel_allocator_t *allocator;
    sixel_output_t *output;
    sixel_wire_color_t wire;
    sixel_index_t pixel;
    SIXELSTATUS status;
    int saved_threads;
    int ok;

    allocator = NULL;
    output = NULL;
    ok = 0;
    pixel = 0;
    memset(&wire, 0, sizeof(wire));
    wire.definition.register_index = reg;
    sink.body = expected;
    sink.prefix = packet ? SIXEL_OUTPUT_PACKET_SIZE - 2u : 0;
    sink.position = 0;
    sink.length = strlen(expected);
    sink.ok = 1;
    saved_threads = sixel_threads_resolve();
    sixel_set_encoder_threads(1);
    status = sixel_allocator_new(&allocator, NULL, NULL, NULL, NULL);
    if (SIXEL_FAILED(status)) goto end;
    status = sixel_output_new(&output, cpu_register_write, &sink, allocator);
    if (SIXEL_FAILED(status)) goto end;
    output->wire_colors = &wire;
    /* Place '#' two bytes before the packet end; decimal digits straddle
     * the flush. The retained suffix and callback form one wire stream. */
    memset(output->buffer, '?', sink.prefix);
    output->pos = (int)sink.prefix;
    status = sixel_encode_body(&pixel, 1, 1, NULL, NULL,
        1, -1, 1, output, NULL, allocator, 0, NULL);
    if (SIXEL_FAILED(status)) goto end;
    cpu_register_write((char *)output->buffer, output->pos, &sink);
    ok = sink.ok && sink.position == sink.prefix + sink.length;
    if (!ok) fprintf(stderr, "register %u: incorrect wire spelling\n", reg);
end:
    sixel_output_unref(output);
    sixel_allocator_unref(allocator);
    sixel_set_encoder_threads(saved_threads);
    return ok ? 0 : 1;
}
