/*
 * SPDX-License-Identifier: MIT
 *
 * Verify that a frame format request cannot report success while retaining
 * byte RGB storage.  The allocator rejects the large float buffer before the
 * conversion loop reads the intentionally tiny borrowed source.
 */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>

#include <sixel.h>

#include "src/frame.h"

#define FPF_ALLOCATION_LIMIT 4096u
#define FPF_WIDTH 4096
#define FPF_HEIGHT 1366

static void *
fpf_malloc(size_t size)
{
    if (size > FPF_ALLOCATION_LIMIT) {
        return NULL;
    }
    return malloc(size);
}

static void *
fpf_calloc(size_t count, size_t size)
{
    if (size != 0u && count > FPF_ALLOCATION_LIMIT / size) {
        return NULL;
    }
    return calloc(count, size);
}

static void *
fpf_realloc(void *ptr, size_t size)
{
    if (size > FPF_ALLOCATION_LIMIT) {
        return NULL;
    }
    return realloc(ptr, size);
}

static int
frame_float_request_fails(void)
{
    SIXELSTATUS status;
    sixel_allocator_t *allocator;
    sixel_frame_t *frame;
    unsigned char pixels[3];

    status = SIXEL_FALSE;
    allocator = NULL;
    frame = NULL;
    pixels[0] = 0u;
    pixels[1] = 0u;
    pixels[2] = 0u;

    status = sixel_allocator_new(&allocator,
                                 fpf_malloc,
                                 fpf_calloc,
                                 fpf_realloc,
                                 free);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    status = sixel_frame_new(&frame, allocator);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    status = sixel_frame_init_borrowed(frame,
                                       pixels,
                                       FPF_WIDTH,
                                       FPF_HEIGHT,
                                       SIXEL_PIXELFORMAT_RGB888,
                                       NULL,
                                       -1);
    if (SIXEL_FAILED(status)) {
        goto end;
    }

    status = sixel_frame_set_pixelformat(
        frame,
        SIXEL_PIXELFORMAT_OKLABFLOAT32);
    if (status != SIXEL_BAD_ALLOCATION) {
        status = SIXEL_LOGIC_ERROR;
        goto end;
    }
    if (sixel_frame_get_pixelformat(frame) != SIXEL_PIXELFORMAT_RGB888 ||
            sixel_frame_get_colorspace(frame) != SIXEL_COLORSPACE_GAMMA) {
        status = SIXEL_LOGIC_ERROR;
        goto end;
    }

    status = SIXEL_OK;

end:
    sixel_frame_unref(frame);
    sixel_allocator_unref(allocator);
    return SIXEL_SUCCEEDED(status);
}

int
test_frame_0002_float_request(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    if (!frame_float_request_fails()) {
        fprintf(stderr, "frame float request failure contract failed\n");
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
