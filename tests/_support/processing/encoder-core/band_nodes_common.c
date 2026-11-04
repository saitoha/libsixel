/* SPDX-License-Identifier: MIT */
/* Allocation probes use one fixed raster per observation. All counters are
 * atomic because parallel band workers share the caller's allocator. */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sixel.h>
#include "src/encoder-core-private.h"
#include "src/threading.h"

static sixel_atomic_u32_t band_nodes_live;
static sixel_atomic_u32_t band_nodes_calls;
static sixel_atomic_u32_t band_nodes_failed;
static sixel_atomic_u32_t band_nodes_writes;
static unsigned int band_nodes_fail_at;
static int band_nodes_armed;
static int band_nodes_reject;

static void *
band_nodes_malloc(size_t size)
{
    void *p;
    unsigned int call;

    /* This fixture's other allocator-backed malloc sizes are output storage,
     * 53 column pointers, seven flags and seven ints: none is a node size.
     * calloc is counted separately, not mistaken for a node allocation. */
    if (band_nodes_armed && size == sizeof(sixel_node_t)) {
        call = sixel_atomic_fetch_add_u32(&band_nodes_calls, 1) + 1;
        if (call == band_nodes_fail_at) {
            (void)sixel_atomic_fetch_add_u32(&band_nodes_failed, 1);
            return NULL;
        }
    }
    p = malloc(size);
    if (p != NULL) (void)sixel_atomic_fetch_add_u32(&band_nodes_live, 1);
    return p;
}

static void *
band_nodes_calloc(size_t count, size_t size)
{
    void *p;

    p = calloc(count, size);
    if (p != NULL) (void)sixel_atomic_fetch_add_u32(&band_nodes_live, 1);
    return p;
}

static void *
band_nodes_realloc(void *old, size_t size)
{
    void *p;
    int was_null;

    was_null = old == NULL;
    p = realloc(old, size);
    if (p != NULL && was_null) {
        (void)sixel_atomic_fetch_add_u32(&band_nodes_live, 1);
    }
    return p;
}

static void
band_nodes_free(void *p)
{
    if (p != NULL) (void)sixel_atomic_fetch_sub_u32(&band_nodes_live, 1);
    free(p);
}

static int
band_nodes_write(char *bytes, int size, void *context)
{
    (void)bytes;
    (void)context;
    (void)sixel_atomic_fetch_add_u32(&band_nodes_writes, 1);
    return band_nodes_reject ? -1 : size;
}

static SIXELSTATUS
band_nodes_sink(char const *bytes, size_t size, void *context)
{
    (void)bytes;
    (void)size;
    (void)context;
    (void)sixel_atomic_fetch_add_u32(&band_nodes_writes, 1);
    return SIXEL_RUNTIME_ERROR;
}

/* kind selects one observation, each registered by its own C/TAP case:
 * 0 bounded node allocations; 1 successful drain releases resources;
 * 2 allocation status; 3 allocation failure releases resources;
 * 4 retry's exact body; 5 rejected sink releases resources;
 * 6 partial same-column chain releases resources. */
int
test_band_nodes_resources(int kind, int threads)
{
    sixel_index_t pixels[257 * 241];
    sixel_allocator_t *allocator;
    sixel_output_t *output;
    SIXELSTATUS status;
    int width;
    int height;
    int saved_threads;
    int x;
    int y;
    int ok;
    unsigned int calls;

#if !SIXEL_ENABLE_THREADS
    if (threads > 1) return 77;
#endif
    allocator = NULL;
    output = NULL;
    ok = 0;
    band_nodes_live = band_nodes_calls = band_nodes_failed = 0;
    band_nodes_writes = 0;
    band_nodes_armed = 0;
    band_nodes_fail_at = 0;
    band_nodes_reject = kind == 5;
    width = kind == 5 ? 257 : 53;
    height = kind == 2 || kind == 4 ? 6 : 241;
    saved_threads = sixel_threads_resolve();
    sixel_set_encoder_threads(threads);
    memset(pixels, 6, sizeof(pixels));
    for (y = 0; y < height; ++y) {
        pixels[y * width] = 0;
        if (kind >= 2 && kind <= 5) pixels[y * width + 13] = 1;
        if (kind == 6) pixels[y * width] = (sixel_index_t)(y % 3);
        if (kind == 5) {
            for (x = 0; x < width; ++x) {
                pixels[y * width + x] = (sixel_index_t)((x + y) % 6);
            }
        }
    }
    status = sixel_allocator_new(&allocator, band_nodes_malloc,
        band_nodes_calloc, band_nodes_realloc, band_nodes_free);
    if (status != SIXEL_OK) goto end;
    status = sixel_output_new(&output, band_nodes_write, NULL, allocator);
    if (status != SIXEL_OK) goto end;
    output->encode_policy = SIXEL_ENCODEPOLICY_FAST;
    if (kind == 5) output->body_sink.write = band_nodes_sink;
    band_nodes_armed = 1;
    if (kind >= 2 && kind <= 4) band_nodes_fail_at = 2;
    if (kind == 6) band_nodes_fail_at = 3;
    status = sixel_encode_body(pixels, width, height, NULL, NULL, 7, 6, 1,
        output, NULL, allocator, 0, NULL);
    calls = sixel_atomic_fetch_add_u32(&band_nodes_calls, 0);
    if ((kind >= 2 && kind <= 4) || kind == 6) {
        if (status != SIXEL_BAD_ALLOCATION ||
                sixel_atomic_fetch_add_u32(&band_nodes_failed, 0) != 1) {
            goto end;
        }
    } else if (kind != 5 && status != SIXEL_OK) {
        goto end;
    }
    if (kind == 0) {
        /* Forty-one bands need one node each. At most one node per worker
         * may be allocated; scheduling need not use every worker. */
        ok = calls > 0 && calls <= (unsigned int)threads;
    } else if (kind == 4) {
        band_nodes_fail_at = 0;
        status = sixel_encode_body(pixels, width, height, NULL, NULL, 7, 6,
            1, output, NULL, allocator, 0, NULL);
        ok = status == SIXEL_OK && output->pos == 10 &&
            memcmp(output->buffer, "#0~#1!12?~", 10) == 0;
    } else if (kind == 5) {
        ok = output->body_status == SIXEL_RUNTIME_ERROR &&
            sixel_atomic_fetch_add_u32(&band_nodes_writes, 0) > 0;
    } else {
        ok = 1;
    }
end:
    sixel_output_unref(output);
    sixel_allocator_unref(allocator);
    sixel_set_encoder_threads(saved_threads);
    band_nodes_armed = 0;
    if (kind == 1 || kind == 3 || kind == 5 || kind == 6) {
        ok = ok && sixel_atomic_fetch_add_u32(&band_nodes_live, 0) == 0;
    }
    if (!ok) {
        fprintf(stderr, "band nodes kind=%d threads=%d live=%u nodes=%u "
            "failed=%u status=%x\n", kind, threads,
            sixel_atomic_fetch_add_u32(&band_nodes_live, 0),
            sixel_atomic_fetch_add_u32(&band_nodes_calls, 0),
            sixel_atomic_fetch_add_u32(&band_nodes_failed, 0), status);
    }
    return ok ? 0 : 1;
}

/* Each caller supplies one fixed raster and an independently derived body.
 * These small bodies fit one packet, so no flushed bytes may be discarded. */
int
test_band_nodes_wire(sixel_index_t *pixels, int width, int height,
    int ncolors, int keycolor, int policy, int left, int top, int threads,
    unsigned char const *mask, char const *expected)
{
    sixel_allocator_t *allocator;
    sixel_output_t *output;
    SIXELSTATUS status;
    int saved_threads;
    int ok;

#if !SIXEL_ENABLE_THREADS
    if (threads > 1) return 77;
#endif
    allocator = NULL;
    output = NULL;
    ok = 0;
    band_nodes_writes = 0;
    band_nodes_reject = 0;
    saved_threads = sixel_threads_resolve();
    sixel_set_encoder_threads(threads);
    status = sixel_allocator_new(&allocator, NULL, NULL, NULL, NULL);
    if (SIXEL_FAILED(status)) goto end;
    status = sixel_output_new(&output, band_nodes_write, NULL, allocator);
    if (SIXEL_FAILED(status)) goto end;
    output->encode_policy = policy;
    output->transparent_offset_left = left;
    output->transparent_offset_top = top;
    output->transparent_mask = mask;
    status = sixel_encode_body(pixels, width, height, NULL, NULL,
        ncolors, keycolor, 1, output, NULL, allocator, 0, NULL);
    if (SIXEL_FAILED(status) || output->pos < 0 ||
            output->pos >= SIXEL_OUTPUT_PACKET_SIZE ||
            sixel_atomic_fetch_add_u32(&band_nodes_writes, 0) != 0) goto end;
    output->buffer[output->pos] = '\0';
    ok = strcmp((char *)output->buffer, expected) == 0;
    if (!ok) fprintf(stderr, "body [%s], expected [%s]\n",
        output->buffer, expected);
end:
    sixel_output_unref(output);
    sixel_allocator_unref(allocator);
    sixel_set_encoder_threads(saved_threads);
    return ok ? 0 : 1;
}
