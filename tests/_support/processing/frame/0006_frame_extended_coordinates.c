/* SPDX-License-Identifier: MIT */
/* Test frame conversion against independent Decimal coordinates, not a
 * production roundtrip. Each direction is a separate TAP observation. */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sixel.h>
#include <6cells.h>
#include "tests/data/color-numeric/opponents.h"
#include "tests/data/color-numeric/trc.h"

int
test_frame_0006_extended(int argc, char **argv)
{
    static int const formats[] = {
        SIXEL_PIXELFORMAT_RGBFLOAT32,
        SIXEL_PIXELFORMAT_OKLABFLOAT32,
        SIXEL_PIXELFORMAT_CIELABFLOAT32,
        SIXEL_PIXELFORMAT_DIN99DFLOAT32
    };
    static int const spaces[] = {
        SIXEL_COLORSPACE_GAMMA, SIXEL_COLORSPACE_OKLAB,
        SIXEL_COLORSPACE_CIELAB, SIXEL_COLORSPACE_DIN99D
    };
    float pixels[sizeof(color_trc) / sizeof(color_trc[0]) * 3];
    double expected[sizeof(pixels) / sizeof(pixels[0])];
    sixel_frame_t *frame;
    sixel_frame_interface_t *iface;
    sixel_frame_pixels_view_t view;
    SIXELSTATUS status;
    size_t count;
    size_t i;
    int c;
    int index;
    int inverse;
    int source;
    int target;
    int result;
    double error;

    if (argc != 3 || strlen(argv[1]) != 1 || argv[1][0] < '0'
            || argv[1][0] > '3' || (strcmp(argv[2], "forward") != 0
            && strcmp(argv[2], "inverse") != 0)) {
        return EXIT_FAILURE;
    }
    index = argv[1][0] - '0';
    inverse = strcmp(argv[2], "inverse") == 0;
    count = index == 0 ? sizeof(color_trc) / sizeof(color_trc[0])
        : sizeof(color_opponents) / sizeof(color_opponents[0]);
    for (i = 0; i < count; ++i) {
        for (c = 0; c < 3; ++c) {
            if (index == 0) {
                pixels[i * 3 + c] = (float)color_trc[i][0];
                expected[i * 3 + c] = color_trc[i][inverse ? 1 : 2];
            } else {
                pixels[i * 3 + c] = (float)
                    color_opponents[i][(inverse ? index * 3 : 0) + c];
                expected[i * 3 + c] =
                    color_opponents[i][(inverse ? 0 : index * 3) + c];
            }
        }
    }
    source = inverse ? formats[index] : SIXEL_PIXELFORMAT_LINEARRGBFLOAT32;
    target = inverse ? SIXEL_PIXELFORMAT_LINEARRGBFLOAT32 : formats[index];
    frame = NULL;
    result = EXIT_FAILURE;
    status = sixel_frame_new(&frame, NULL);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    status = sixel_frame_init_borrowed(frame, pixels, (int)count, 1,
                                      source, NULL, -1);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    iface = sixel_frame_as_interface(frame);
    status = iface->vtbl->set_pixelformat(iface, target);
    if (SIXEL_FAILED(status)) {
        fprintf(stderr, "coordinate conversion failed: %x\n", status);
        goto end;
    }
    status = iface->vtbl->get_pixels(iface, &view);
    if (SIXEL_FAILED(status) || view.pixels != NULL
            || view.pixels_float32 == NULL || view.pixelformat != target
            || view.colorspace != (inverse ? SIXEL_COLORSPACE_LINEAR
                                           : spaces[index])
            || view.width != (int)count || view.height != 1) {
        goto end;
    }
    for (i = 0; i < count * 3; ++i) {
        error = fabs((double)view.pixels_float32[i] - expected[i]);
        /* Rounded opponent matrices and float32 input/output have their
         * own error budget; byte reconstruction is much larger. */
        if (!isfinite(view.pixels_float32[i]) || error > 1e-6) {
            fprintf(stderr, "component %lu: %.9g expected %.17g\n",
                (unsigned long)i, view.pixels_float32[i], expected[i]);
            goto end;
        }
    }
    result = EXIT_SUCCESS;
end:
    sixel_frame_unref(frame);
    return result;
}
