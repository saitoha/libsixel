/* SPDX-License-Identifier: MIT */
/* A byte derivative belongs to a clone, never to the canonical float frame. */
#include <stdlib.h>
#include <string.h>
#include <sixel.h>
#include <6cells.h>
#include "tests/data/color-numeric/opponents.h"

int
test_frame_0014_clone_float(int argc, char **argv)
{
    float pixels[30];
    float original[30];
    sixel_frame_t *frame;
    sixel_frame_interface_t *iface;
    sixel_frame_interface_t *clone;
    sixel_frame_pixels_view_t view;
    SIXELSTATUS status;
    size_t i;
    int c;
    int result;

    (void)argc;
    (void)argv;
    for (i = 0; i < 10; ++i) {
        for (c = 0; c < 3; ++c) {
            pixels[i * 3 + c] = (float)color_opponents[i][c];
        }
    }
    memcpy(original, pixels, sizeof(pixels));
    frame = NULL;
    clone = NULL;
    result = EXIT_FAILURE;
    status = sixel_frame_new(&frame, NULL);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    status = sixel_frame_init_borrowed(frame, pixels, 10, 1,
        SIXEL_PIXELFORMAT_LINEARRGBFLOAT32, NULL, -1);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    iface = sixel_frame_as_interface(frame);
    status = iface->vtbl->clone(iface, NULL, &clone);
    if (SIXEL_FAILED(status) || clone == NULL) {
        goto end;
    }
    status = clone->vtbl->get_pixels(clone, &view);
    if (SIXEL_FAILED(status) || view.pixels != NULL
            || view.pixels_float32 == NULL || view.pixels_float32 == pixels
            || view.pixelformat != SIXEL_PIXELFORMAT_LINEARRGBFLOAT32
            || view.colorspace != SIXEL_COLORSPACE_LINEAR
            || memcmp(view.pixels_float32, original, sizeof(original)) != 0) {
        goto end;
    }
    status = clone->vtbl->set_pixelformat(clone, SIXEL_PIXELFORMAT_RGB888);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    status = clone->vtbl->get_pixels(clone, &view);
    if (SIXEL_FAILED(status) || view.pixels_float32 != NULL
            || view.pixels == NULL
            || view.pixelformat != SIXEL_PIXELFORMAT_RGB888
            || view.colorspace != SIXEL_COLORSPACE_GAMMA
            || view.pixels[0] != 255 || view.pixels[1] != 0
            || view.pixels[2] != 0) {
        goto end;
    }
    status = iface->vtbl->get_pixels(iface, &view);
    if (SIXEL_FAILED(status) || view.pixels != NULL
            || view.pixels_float32 != pixels
            || view.pixelformat != SIXEL_PIXELFORMAT_LINEARRGBFLOAT32
            || view.colorspace != SIXEL_COLORSPACE_LINEAR
            || memcmp(view.pixels_float32, original, sizeof(original)) != 0) {
        goto end;
    }
    result = EXIT_SUCCESS;
end:
    if (clone != NULL) {
        clone->vtbl->unref(clone);
    }
    sixel_frame_unref(frame);
    return result;
}
