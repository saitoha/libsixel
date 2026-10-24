/* SPDX-License-Identifier: MIT */
/* An occupancy allocation failure releases scratch space and permits reuse. */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sixel.h>
#include "src/encoder-core-private.h"
#include "src/threading.h"

SIXELSTATUS sixel_encode_body(sixel_index_t *, int, int, unsigned char *,
    float const *, int, int, int, sixel_output_t *, unsigned char *,
    sixel_allocator_t *, int, sixel_timeline_logger_t *);

static size_t compose_alloc_live;
static int compose_alloc_fail;
static int compose_alloc_failed;

static void *
compose_alloc_malloc(size_t size)
{
    void *p;

    p = malloc(size);
    if (p != NULL) {
        ++compose_alloc_live;
    }
    return p;
}

static void *
compose_alloc_calloc(size_t count, size_t size)
{
    void *p;

    /* Three colors, each with three words for 130 encoded columns. */
    if (compose_alloc_fail && count == 9 && size == sizeof(uint64_t)) {
        ++compose_alloc_failed;
        return NULL;
    }
    p = compose_alloc_malloc(count * size);
    if (p != NULL) {
        memset(p, 0, count * size);
    }
    return p;
}

static void *
compose_alloc_realloc(void *old, size_t size)
{
    void *p;
    int was_null;

    was_null = old == NULL;
    p = realloc(old, size);
    if (p != NULL && was_null) {
        ++compose_alloc_live;
    }
    return p;
}

static void
compose_alloc_free(void *p)
{
    if (p != NULL) {
        --compose_alloc_live;
    }
    free(p);
}

static int
compose_alloc_write(char *data, int size, void *opaque)
{
    (void)data;
    (void)opaque;
    return size;
}

int
test_compose_0049(int argc, char **argv)
{
    sixel_allocator_t *allocator;
    sixel_output_t *output;
    sixel_index_t pixels[130];
    SIXELSTATUS status;
    size_t baseline;
    int saved_threads;
    int ok;

    (void)argc;
    (void)argv;
    allocator = NULL;
    output = NULL;
    ok = 0;
    compose_alloc_live = 0;
    compose_alloc_fail = 0;
    compose_alloc_failed = 0;
    saved_threads = sixel_threads_resolve();
    sixel_set_encoder_threads(1);
    memset(pixels, 2, sizeof(pixels));
    pixels[0] = 0;
    status = sixel_allocator_new(&allocator, compose_alloc_malloc,
        compose_alloc_calloc, compose_alloc_realloc, compose_alloc_free);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    status = sixel_output_new(&output, compose_alloc_write, NULL, allocator);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    baseline = compose_alloc_live;
    compose_alloc_fail = 1;
    status = sixel_encode_body(pixels, 130, 1, NULL, NULL, 3, 2, 1,
        output, NULL, allocator, 0, NULL);
    if (status != SIXEL_BAD_ALLOCATION || compose_alloc_failed != 1 ||
            compose_alloc_live != baseline || output->pos != 0) {
        goto end;
    }
    compose_alloc_fail = 0;
    status = sixel_encode_body(pixels, 130, 1, NULL, NULL, 3, 2, 1,
        output, NULL, allocator, 0, NULL);
    ok = SIXEL_SUCCEEDED(status) && compose_alloc_live == baseline &&
        output->pos == 3 && memcmp(output->buffer, "#0@", 3) == 0;
end:
    compose_alloc_fail = 0;
    sixel_output_unref(output);
    sixel_allocator_unref(allocator);
    sixel_set_encoder_threads(saved_threads);
    return ok && compose_alloc_live == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
