/* SPDX-License-Identifier: MIT */
/* Reject a bad final triplet without converting the valid prefix or tags. */
#include <float.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <sixel.h>
#include <6cells.h>

int
test_frame_0015_atomic(int argc, char **argv)
{
    float pixels[9] = {0.2f, 0.4f, 0.6f, 0.8f, 0.3f, 0.1f,
                       0.5f, 0.5f, 0.5f};
    float original[9];
    sixel_frame_t *frame;
    sixel_frame_interface_t *iface;
    sixel_frame_pixels_view_t view;
    SIXELSTATUS status;
    int source;
    int target;
    int colorspace;
    int result;

    if (argc != 2) {
        return EXIT_FAILURE;
    }
    source = SIXEL_PIXELFORMAT_LINEARRGBFLOAT32;
    colorspace = SIXEL_COLORSPACE_LINEAR;
    target = SIXEL_PIXELFORMAT_OKLABFLOAT32;
    if (strcmp(argv[1], "nonfinite") == 0) {
        pixels[8] = NAN;
    } else if (strcmp(argv[1], "domain") == 0) {
        pixels[6] = pixels[7] = pixels[8] = -1.0f;
        target = SIXEL_PIXELFORMAT_DIN99DFLOAT32;
    } else if (strcmp(argv[1], "overflow") == 0) {
        pixels[8] = FLT_MAX;
        source = SIXEL_PIXELFORMAT_RGBFLOAT32;
        colorspace = SIXEL_COLORSPACE_GAMMA;
        target = SIXEL_PIXELFORMAT_LINEARRGBFLOAT32;
    } else {
        return EXIT_FAILURE;
    }
    memcpy(original, pixels, sizeof(pixels));
    frame = NULL;
    result = EXIT_FAILURE;
    status = sixel_frame_new(&frame, NULL);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    status = sixel_frame_init_borrowed(frame, pixels, 3, 1,
                                      source, NULL, -1);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    iface = sixel_frame_as_interface(frame);
    status = iface->vtbl->set_pixelformat(iface, target);
    if (status != SIXEL_BAD_INPUT) {
        goto end;
    }
    status = iface->vtbl->get_pixels(iface, &view);
    if (SIXEL_FAILED(status) || view.pixels != NULL
            || view.pixels_float32 != pixels || view.pixelformat != source
            || view.colorspace != colorspace || view.width != 3
            || view.height != 1
            || memcmp(view.pixels_float32, original, sizeof(original)) != 0) {
        goto end;
    }
    result = EXIT_SUCCESS;
end:
    sixel_frame_unref(frame);
    return result;
}
