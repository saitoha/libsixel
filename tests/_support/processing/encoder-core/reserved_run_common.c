/* SPDX-License-Identifier: MIT */
/* Observe one literal wire stream, including any flushed packet prefix. */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
#include <stdio.h>
#include <string.h>
#include <sixel.h>
#include "src/encoder-core-private.h"
#include "src/threading.h"

struct reserved_run_sink {
    char const *body;
    size_t prefix;
    size_t length;
    size_t position;
    int ok;
};

static int
reserved_run_write(char *data, int size, void *opaque)
{
    struct reserved_run_sink *sink;
    size_t position;
    char expected;
    int i;

    sink = (struct reserved_run_sink *)opaque;
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
test_reserved_run(int width, int limited, int packet_tail, int left,
    char const *expected)
{
    struct reserved_run_sink sink;
    sixel_allocator_t *allocator;
    sixel_output_t *output;
    sixel_index_t *pixels;
    SIXELSTATUS status;
    int saved_threads;
    int ok;

    allocator = NULL;
    output = NULL;
    pixels = NULL;
    ok = 0;
    saved_threads = sixel_threads_resolve();
    sixel_set_encoder_threads(1);
    sink.body = expected;
    sink.prefix = packet_tail == 0 ? 0 :
        (size_t)(SIXEL_OUTPUT_PACKET_SIZE - packet_tail);
    sink.length = strlen(expected);
    sink.position = 0;
    sink.ok = 1;
    status = sixel_allocator_new(&allocator, NULL, NULL, NULL, NULL);
    if (SIXEL_FAILED(status)) goto end;
    pixels = (sixel_index_t *)sixel_allocator_malloc(allocator,
        (size_t)width * sizeof(*pixels));
    if (pixels == NULL) goto end;
    /* A one-row monochrome raster emits '@' for each source column.
     * Transparent offset columns emit '?'; the selector is omitted. */
    memset(pixels, 1, (size_t)width * sizeof(*pixels));
    status = sixel_output_new(&output, reserved_run_write, &sink, allocator);
    if (SIXEL_FAILED(status)) goto end;
    output->has_gri_arg_limit = limited;
    output->transparent_offset_left = left;
    memset(output->buffer, '?', sink.prefix);
    output->pos = (int)sink.prefix;
    status = sixel_encode_body(pixels, width, 1, NULL, NULL,
        2, 0, 1, output, NULL, allocator, 0, NULL);
    if (SIXEL_FAILED(status)) goto end;
    reserved_run_write((char *)output->buffer, output->pos, &sink);
    ok = sink.ok && sink.position == sink.prefix + sink.length;
    if (!ok) fprintf(stderr, "reserved run: incorrect wire bytes\n");
end:
    sixel_output_unref(output);
    if (allocator != NULL) sixel_allocator_free(allocator, pixels);
    sixel_allocator_unref(allocator);
    sixel_set_encoder_threads(saved_threads);
    return ok ? 0 : 1;
}
