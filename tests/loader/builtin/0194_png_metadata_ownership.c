/* SPDX-License-Identifier: MIT */
/* Keep component ownership balanced across PNG metadata and callback failures.
 * Numeric source interpretation is asserted independently in the sample tests;
 * this test uses real PNG decoding and the component's allocation callbacks. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "src/cms.h"
#include "loader_builtin_memory_test_common.h"

static int pmo_live;
static int pmo_bad_free;

static void *
pmo_malloc(size_t size)
{
    void *p;
    p = malloc(size);
    if (p != NULL) { ++pmo_live; }
    return p;
}

static void *
pmo_calloc(size_t count, size_t size)
{
    void *p;
    if (count && size > SIZE_MAX / count) { return NULL; }
    p = pmo_malloc(count * size);
    if (p != NULL) { memset(p, 0, count * size); }
    return p;
}

static void *
pmo_realloc(void *p, size_t size)
{
    return p == NULL ? pmo_malloc(size) : realloc(p, size);
}

static void
pmo_free(void *p)
{
    if (p != NULL) {
        if (pmo_live <= 0) { pmo_bad_free = 1; }
        --pmo_live;
    }
    free(p);
}

static SIXELSTATUS
pmo_callback(sixel_frame_t *frame, void *context)
{
    int *calls;
    calls = context;
    if (frame == NULL || calls == NULL) { return SIXEL_BAD_ARGUMENT; }
    ++*calls;
    return SIXEL_BAD_INPUT;
}

int
test_loader_0194_png_metadata_ownership(int argc, char **argv)
{
    static char const *const paths[] = {
        "/tests/data/color-numeric/png-metadata-boundary-filebg-16.png",
        "/tests/data/color-numeric/png-metadata-singular-filebg-16.png",
        "/tests/data/color-numeric/png-metadata-boundary-filebg-indexed.png"
    };
    sixel_allocator_t *allocator;
    edge_loader_options_t options;
    SIXELSTATUS status;
    int fixture;
    int callback;
    int calls;
    int result;

    if (argc != 3) { return 1; }
    fixture = atoi(argv[1]);
    callback = atoi(argv[2]);
    if (fixture < 0 || fixture >= 3 || callback < 0 || callback > 1) {
        return 1;
    }
    edge_loader_options_init(&options);
    options.require_static = 1;
    options.use_palette = 1;
    options.cms_engine = SIXEL_CMS_ENGINE_BUILTIN;
    options.set_prefer_float32 = 1;
    options.prefer_float32 = 1;
    if (!callback) {
        return edge_expect_fixture_allocation_failures("PNG metadata",
            paths[fixture], &options, 0);
    }
    allocator = NULL;
    pmo_live = pmo_bad_free = 0;
    calls = 0;
    status = sixel_allocator_new(&allocator, pmo_malloc, pmo_calloc,
        pmo_realloc, pmo_free);
    if (SIXEL_FAILED(status)) { return 1; }
    result = edge_load_fixture_custom("PNG metadata callback", paths[fixture],
        &options, allocator, pmo_callback, &calls, &status);
    sixel_allocator_unref(allocator);
    if (result != 0 || status != SIXEL_BAD_INPUT || calls != 1 ||
        pmo_live != 0 || pmo_bad_free) {
        fprintf(stderr, "PNG callback: result=%d status=%x calls=%d live=%d\n",
            result, status, calls, pmo_live);
        return 1;
    }
    return 0;
}
