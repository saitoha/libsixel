/* SPDX-License-Identifier: MIT */
/* A band-column allocation failure releases scratch space and permits reuse. */
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

static size_t classify_alloc_live;
static int classify_alloc_fail;
static int classify_alloc_failed;

static void *
classify_alloc_malloc(size_t size)
{
    void *p;

    /* One packed word for each of the 130 encoded columns. */
    if (classify_alloc_fail && size == 130 * sizeof(uint64_t)) {
        ++classify_alloc_failed;
        return NULL;
    }
    p = malloc(size);
    if (p != NULL) {
        ++classify_alloc_live;
    }
    return p;
}

static void *
classify_alloc_calloc(size_t count, size_t size)
{
    void *p;

    p = classify_alloc_malloc(count * size);
    if (p != NULL) {
        memset(p, 0, count * size);
    }
    return p;
}

static void *
classify_alloc_realloc(void *old, size_t size)
{
    void *p;
    int was_null;

    was_null = old == NULL;
    p = realloc(old, size);
    if (p != NULL && was_null) {
        ++classify_alloc_live;
    }
    return p;
}

static void
classify_alloc_free(void *p)
{
    if (p != NULL) {
        --classify_alloc_live;
    }
    free(p);
}

static int
classify_alloc_write(char *data, int size, void *opaque)
{
    (void)data;
    (void)opaque;
    return size;
}

int
test_classify_0053(int argc, char **argv)
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
    classify_alloc_live = 0;
    classify_alloc_fail = 0;
    classify_alloc_failed = 0;
    saved_threads = sixel_threads_resolve();
    sixel_set_threads(1);
    memset(pixels, 2, sizeof(pixels));
    pixels[0] = 0;
    status = sixel_allocator_new(&allocator, classify_alloc_malloc,
        classify_alloc_calloc, classify_alloc_realloc, classify_alloc_free);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    status = sixel_output_new(&output, classify_alloc_write, NULL, allocator);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    baseline = classify_alloc_live;
    classify_alloc_fail = 1;
    status = sixel_encode_body(pixels, 130, 1, NULL, NULL, 3, 2, 1,
        output, NULL, allocator, 0, NULL);
    if (status != SIXEL_BAD_ALLOCATION || classify_alloc_failed != 1 ||
            classify_alloc_live != baseline || output->pos != 0 ||
            strstr(sixel_helper_get_additional_message(),
                   "band column allocation failed") == NULL) {
        goto end;
    }
    classify_alloc_fail = 0;
    status = sixel_encode_body(pixels, 130, 1, NULL, NULL, 3, 2, 1,
        output, NULL, allocator, 0, NULL);
    ok = SIXEL_SUCCEEDED(status) && classify_alloc_live == baseline &&
        output->pos == 3 && memcmp(output->buffer, "#0@", 3) == 0;
end:
    classify_alloc_fail = 0;
    sixel_output_unref(output);
    sixel_allocator_unref(allocator);
    sixel_set_threads(saved_threads);
    return ok && classify_alloc_live == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
