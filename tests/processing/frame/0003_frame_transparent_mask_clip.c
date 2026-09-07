/*
 * SPDX-License-Identifier: MIT
 *
 * Policy: docs/concepts/pixelformat.md
 *
 * Verify that clipping a frame applies the same geometry to its transparent
 * mask and preserves the semantic alpha-zero marker.
 */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sixel.h>
#include <6cells.h>

static SIXELSTATUS
frame_mask_clip_init(sixel_allocator_t *allocator, sixel_frame_t **frame_out)
{
    static unsigned char const source_pixels[12] = {
        0xffu, 0x00u, 0x00u,
        0xffu, 0x00u, 0x00u,
        0xffu, 0x00u, 0x00u,
        0xffu, 0x00u, 0x00u
    };
    static unsigned char const source_mask[4] = {
        1u, 0u, 1u, 0u
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
test_frame_0003_transparent_mask_clip(int argc, char **argv)
{
    static unsigned char const expected_mask[2] = {
        0u, 1u
    };
    SIXELSTATUS status;
    sixel_allocator_t *allocator;
    sixel_frame_t *frame;
    sixel_frame_interface_t *frame_if;
    sixel_frame_transparency_t transparency;

    (void)argc;
    (void)argv;
    status = SIXEL_FALSE;
    allocator = NULL;
    frame = NULL;
    frame_if = NULL;
    memset(&transparency, 0, sizeof(transparency));

    status = sixel_allocator_new(&allocator, NULL, NULL, NULL, NULL);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    status = frame_mask_clip_init(allocator, &frame);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    status = sixel_frame_clip(frame, 1, 0, 2, 1);
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
        transparency.transparent_mask_size != sizeof(expected_mask) ||
        memcmp(transparency.transparent_mask,
               expected_mask,
               sizeof(expected_mask)) != 0) {
        status = SIXEL_LOGIC_ERROR;
        goto end;
    }
    status = SIXEL_OK;

end:
    sixel_frame_unref(frame);
    sixel_allocator_unref(allocator);
    if (SIXEL_FAILED(status)) {
        fprintf(stderr, "frame transparent mask clip contract failed\n");
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
