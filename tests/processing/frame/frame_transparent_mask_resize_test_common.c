/*
 * SPDX-License-Identifier: MIT
 *
 * Shared transparent-mask resize test implementation.
 */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sixel.h>
#include <6cells.h>

#include "tests/processing/frame/frame_transparent_mask_resize_test_common.h"

static SIXELSTATUS
frame_mask_resize_init(sixel_allocator_t *allocator,
                       sixel_frame_t **frame_out)
{
    static unsigned char const source_pixels[12] = {
        0xffu, 0x00u, 0x00u,
        0xffu, 0x00u, 0x00u,
        0xffu, 0x00u, 0x00u,
        0xffu, 0x00u, 0x00u
    };
    static unsigned char const source_mask[4] = {
        1u, 1u, 0u, 0u
    };
    SIXELSTATUS status;
    sixel_frame_t *frame;
    sixel_frame_interface_t *frame_if;
    sixel_frame_transparency_t transparency;
    unsigned char *pixels;
    unsigned char *mask;

    status = SIXEL_FALSE;
    frame = NULL;
    frame_if = NULL;
    memset(&transparency, 0, sizeof(transparency));
    pixels = NULL;
    mask = NULL;
    *frame_out = NULL;

    status = sixel_frame_new(&frame, allocator);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    pixels = (unsigned char *)sixel_allocator_malloc(
        allocator,
        sizeof(source_pixels));
    if (pixels == NULL) {
        status = SIXEL_BAD_ALLOCATION;
        goto end;
    }
    memcpy(pixels, source_pixels, sizeof(source_pixels));
    status = sixel_frame_init(frame,
                              pixels,
                              4,
                              1,
                              SIXEL_PIXELFORMAT_RGB888,
                              NULL,
                              (-1));
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    pixels = NULL;
    mask = (unsigned char *)sixel_allocator_malloc(allocator, 4u);
    if (mask == NULL) {
        status = SIXEL_BAD_ALLOCATION;
        goto end;
    }
    memcpy(mask, source_mask, sizeof(source_mask));
    transparency.transparent = (-1);
    transparency.alpha_zero_is_transparent = 1;
    transparency.transparent_mask = mask;
    transparency.transparent_mask_size = sizeof(source_mask);
    frame_if = sixel_frame_as_interface(frame);
    status = frame_if->vtbl->set_transparency(frame_if, &transparency);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    mask = NULL;
    *frame_out = frame;
    frame = NULL;

end:
    sixel_allocator_free(allocator, pixels);
    sixel_allocator_free(allocator, mask);
    sixel_frame_unref(frame);
    return status;
}

int
frame_transparent_mask_resize_run(
    int method_for_resampling,
    unsigned char const expected_mask[7])
{
    SIXELSTATUS status;
    sixel_allocator_t *allocator;
    sixel_frame_t *frame;
    sixel_frame_interface_t *frame_if;
    sixel_frame_transparency_t transparency;

    status = SIXEL_FALSE;
    allocator = NULL;
    frame = NULL;
    frame_if = NULL;
    memset(&transparency, 0, sizeof(transparency));
    status = sixel_allocator_new(&allocator, NULL, NULL, NULL, NULL);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    status = frame_mask_resize_init(allocator, &frame);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    status = sixel_frame_resize(frame, 7, 1, method_for_resampling);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    frame_if = sixel_frame_as_interface(frame);
    status = frame_if->vtbl->get_transparency(frame_if, &transparency);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    if (transparency.alpha_zero_is_transparent == 0 ||
        transparency.transparent_mask == NULL ||
        transparency.transparent_mask_size != 7u ||
        memcmp(transparency.transparent_mask, expected_mask, 7u) != 0) {
        status = SIXEL_LOGIC_ERROR;
        goto end;
    }
    status = SIXEL_OK;

end:
    sixel_frame_unref(frame);
    sixel_allocator_unref(allocator);
    return SIXEL_SUCCEEDED(status);
}

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
