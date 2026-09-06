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

#include <sixel.h>

#include "src/scale.h"

#define SRAF_BYTE_SCRATCH 24u
#define SRAF_FLOAT_SCRATCH 96u

static void *
sraf_malloc(size_t size)
{
    if (size == SRAF_BYTE_SCRATCH || size == SRAF_FLOAT_SCRATCH) {
        return NULL;
    }
    return malloc(size);
}

static void *
sraf_calloc(size_t count, size_t size)
{
    if (size != 0u
        && ((count == SRAF_BYTE_SCRATCH / size
             && count * size == SRAF_BYTE_SCRATCH)
            || (count == SRAF_FLOAT_SCRATCH / size
                && count * size == SRAF_FLOAT_SCRATCH))) {
        return NULL;
    }
    return calloc(count, size);
}

static void *
sraf_realloc(void *ptr, size_t size)
{
    if (size == SRAF_BYTE_SCRATCH || size == SRAF_FLOAT_SCRATCH) {
        return NULL;
    }
    return realloc(ptr, size);
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

    (void)argc;
    (void)argv;

    status = SIXEL_FALSE;
    allocator = NULL;

    status = sixel_allocator_new(&allocator,
                                 sraf_malloc,
                                 sraf_calloc,
                                 sraf_realloc,
                                 free);
    if (SIXEL_FAILED(status)) {
        goto end;
    }

    status = sixel_helper_scale_image(byte_dst,
                                      byte_src,
                                      2,
                                      2,
                                      SIXEL_PIXELFORMAT_RGB888,
                                      4,
                                      2,
                                      SIXEL_RES_BILINEAR,
                                      allocator);
    if (status != SIXEL_BAD_ALLOCATION) {
        status = SIXEL_LOGIC_ERROR;
        goto end;
    }

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
    if (status != SIXEL_BAD_ALLOCATION) {
        status = SIXEL_LOGIC_ERROR;
        goto end;
    }

    status = SIXEL_OK;

end:
    sixel_allocator_unref(allocator);
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
