/* SPDX-License-Identifier: MIT */
/* A failed RGB palette allocation publishes no frame and leaks no ownership. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "loader_builtin_memory_test_common.h"

static int pal_fail_armed;
static int pal_fail_hits;
static int pal_fail_live;
static int pal_fail_bad_free;

static void *
pal_fail_malloc(size_t size)
{
    void *p;

    if (pal_fail_armed && size == 12) {
        ++pal_fail_hits;
        return NULL;
    }
    p = malloc(size);
    if (p != NULL) {
        ++pal_fail_live;
    }
    return p;
}

static void *
pal_fail_calloc(size_t count, size_t size)
{
    void *p;

    if (count != 0 && size > SIZE_MAX / count) {
        return NULL;
    }
    p = pal_fail_malloc(count * size);
    if (p != NULL) {
        memset(p, 0, count * size);
    }
    return p;
}

static void *
pal_fail_realloc(void *p, size_t size)
{
    void *resized;

    if (p == NULL) {
        return pal_fail_malloc(size);
    }
    if (pal_fail_armed && size == 12) {
        ++pal_fail_hits;
        return NULL;
    }
    resized = realloc(p, size);
    return resized;
}

static void
pal_fail_free(void *p)
{
    if (p != NULL) {
        if (pal_fail_live <= 0) {
            pal_fail_bad_free = 1;
        }
        --pal_fail_live;
    }
    free(p);
}

static SIXELSTATUS
pal_fail_capture(sixel_frame_t *frame, void *context)
{
    int *calls;

    calls = (int *)context;
    if (frame == NULL || calls == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    ++*calls;
    return SIXEL_OK;
}

int
test_loader_0190_png_palette_failure(int argc, char **argv)
{
    sixel_allocator_t *allocator;
    edge_loader_options_t options;
    SIXELSTATUS status;
    int calls;
    int result;
    int pass;

    (void)argc;
    (void)argv;
    for (pass = 0; pass < 2; ++pass) {
        allocator = NULL;
        calls = 0;
        pal_fail_armed = 0;
        pal_fail_hits = 0;
        pal_fail_live = 0;
        pal_fail_bad_free = 0;
        status = sixel_allocator_new(&allocator, pal_fail_malloc,
            pal_fail_calloc, pal_fail_realloc, pal_fail_free);
        if (SIXEL_FAILED(status)) {
            return 1;
        }
        edge_loader_options_init(&options);
        options.require_static = 1;
        options.use_palette = 1;
        pal_fail_armed = pass;
        sixel_helper_set_additional_message("");
        result = edge_load_fixture_custom("RGB palette allocation failure",
            "/tests/data/color-numeric/palette-trns-single.png", &options,
            allocator, pal_fail_capture, &calls, &status);
        pal_fail_armed = 0;
        sixel_allocator_unref(allocator);
        /* The diagnostic proves the size-specific fault reached the palette
         * converter, rather than an earlier allocation or decode fallback. */
        if (result != 0 || pal_fail_live != 0 || pal_fail_bad_free ||
            calls != (pass ? 0 : 1) || pal_fail_hits != pass ||
            status != (pass ? SIXEL_BAD_ALLOCATION : SIXEL_OK) ||
            (pass && strstr(sixel_helper_get_additional_message(),
                "convert_palette_to_rgb:") == NULL)) {
            fprintf(stderr, "palette failure: status=%x calls=%d live=%d "
                "hits=%d\n", status, calls, pal_fail_live, pal_fail_hits);
            return 1;
        }
    }
    return 0;
}
