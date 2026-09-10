/*
 * SPDX-License-Identifier: MIT
 *
 * Verify that byte and float32 resamplers report scratch allocation failure
 * instead of returning success with incomplete output.
 */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <stdlib.h>
#include <string.h>

#include <sixel.h>

#include "src/scale.h"

#define SRAF_BYTE_SCRATCH 24u
#define SRAF_FLOAT_SCRATCH 96u

/*
 * Public allocator callbacks have no userdata argument. The test runner
 * dispatches one test at a time, so this namespaced state can arm one exact
 * allocation without changing the allocator API or affecting setup.
 */
static size_t sraf_failure_size;
static unsigned int sraf_failure_count;
static unsigned int sraf_live_allocation_count;

static int
sraf_should_fail(size_t size)
{
    if (sraf_failure_size != 0u && size == sraf_failure_size) {
        sraf_failure_size = 0u;
        ++sraf_failure_count;
        return 1;
    }
    return 0;
}

static void *
sraf_malloc(size_t size)
{
    void *ptr;

    ptr = NULL;
    if (sraf_should_fail(size)) {
        return NULL;
    }
    ptr = malloc(size);
    if (ptr != NULL) {
        ++sraf_live_allocation_count;
    }
    return ptr;
}

static void *
sraf_calloc(size_t count, size_t size)
{
    size_t bytes;
    void *ptr;

    bytes = 0u;
    ptr = NULL;
    if (size != 0u && count <= (size_t)-1 / size) {
        bytes = count * size;
    }
    if (sraf_should_fail(bytes)) {
        return NULL;
    }
    ptr = calloc(count, size);
    if (ptr != NULL) {
        ++sraf_live_allocation_count;
    }
    return ptr;
}

static void *
sraf_realloc(void *ptr, size_t size)
{
    void *resized;
    int had_allocation;

    resized = NULL;
    had_allocation = ptr != NULL;
    if (sraf_should_fail(size)) {
        return NULL;
    }
    resized = realloc(ptr, size);
    if (!had_allocation && resized != NULL) {
        ++sraf_live_allocation_count;
    }
    return resized;
}

static void
sraf_free(void *ptr)
{
    if (ptr != NULL && sraf_live_allocation_count != 0u) {
        --sraf_live_allocation_count;
    }
    free(ptr);
}

int
test_scale_0003_resample_alloc_failure(int argc, char **argv)
{
    SIXELSTATUS status;
    float float_dst[4 * 2 * 3];
    float float_src[2 * 2 * 3];
    sixel_allocator_t *allocator;
    unsigned char byte_dst[4 * 2 * 3];
    unsigned char byte_src[2 * 2 * 3];
    unsigned int live_allocation_baseline;

    (void)argc;
    (void)argv;

    status = SIXEL_FALSE;
    allocator = NULL;
    memset(float_src, 0, sizeof(float_src));
    memset(byte_src, 0, sizeof(byte_src));
    live_allocation_baseline = 0u;
    sraf_failure_size = 0u;
    sraf_failure_count = 0u;
    sraf_live_allocation_count = 0u;

    status = sixel_allocator_new(&allocator,
                                 sraf_malloc,
                                 sraf_calloc,
                                 sraf_realloc,
                                 sraf_free);
    if (SIXEL_FAILED(status)) {
        goto end;
    }

    live_allocation_baseline = sraf_live_allocation_count;
    sraf_failure_size = SRAF_BYTE_SCRATCH;
    status = sixel_helper_scale_image(byte_dst,
                                      byte_src,
                                      2,
                                      2,
                                      SIXEL_PIXELFORMAT_RGB888,
                                      4,
                                      2,
                                      SIXEL_RES_BILINEAR,
                                      allocator);
    sraf_failure_size = 0u;
    if (status != SIXEL_BAD_ALLOCATION
        || sraf_failure_count != 1u
        || sraf_live_allocation_count != live_allocation_baseline) {
        status = SIXEL_LOGIC_ERROR;
        goto end;
    }

    sraf_failure_count = 0u;
    sraf_failure_size = SRAF_FLOAT_SCRATCH;
    status = sixel_helper_scale_image_float32(
        float_dst,
        float_src,
        2,
        2,
        SIXEL_PIXELFORMAT_LINEARRGBFLOAT32,
        4,
        2,
        SIXEL_RES_BILINEAR,
        allocator);
    sraf_failure_size = 0u;
    if (status != SIXEL_BAD_ALLOCATION
        || sraf_failure_count != 1u
        || sraf_live_allocation_count != live_allocation_baseline) {
        status = SIXEL_LOGIC_ERROR;
        goto end;
    }

    status = SIXEL_OK;

end:
    sraf_failure_size = 0u;
    sixel_allocator_unref(allocator);
    if (sraf_live_allocation_count != 0u) {
        status = SIXEL_LOGIC_ERROR;
    }
    return SIXEL_SUCCEEDED(status) ? EXIT_SUCCESS : EXIT_FAILURE;
}

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
