/* SPDX-License-Identifier: MIT */
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "src/colorspace.h"
#include "src/frame-private.h"
#include "src/loader.h"

typedef struct output_bpc_context {
    int enabled;
    int geometry;
    int calls;
} output_bpc_context_t;

static int
output_bpc_check_geometry(sixel_frame_t *frame, unsigned int history,
                          double expected)
{
    sixel_frame_interface_t *iface;
    sixel_frame_interface_t *copy = NULL;
    sixel_frame_pixels_view_t view = {0};
    size_t i;
    int ok;

    /* Crop a half-covered pixel, then enlarge its constant color plane.
     * The loaded value already includes the file background and BPC. */
    iface = sixel_frame_as_interface(frame);
    ok = SIXEL_SUCCEEDED(iface->vtbl->clone(iface, NULL, &copy)) &&
        SIXEL_SUCCEEDED(copy->vtbl->clip(copy, 3, 0, 1, 1)) &&
        SIXEL_SUCCEEDED(copy->vtbl->resize_float32(copy, 2, 2,
                                                   SIXEL_RES_NEAREST)) &&
        SIXEL_SUCCEEDED(copy->vtbl->get_pixels(copy, &view)) &&
        view.colorspace == SIXEL_COLORSPACE_LINEAR &&
        view.width == 2 && view.height == 2 &&
        ((sixel_frame_t *)copy)->cms_history == history;
    for (i = 0u; ok && i < 12u; ++i) {
        ok = fabs(view.pixels_float32[i] - expected) < 3.0e-5;
    }
    if (copy != NULL) {
        copy->vtbl->unref(copy);
    }
    return ok;
}

static SIXELSTATUS
output_bpc_check_float(sixel_frame_t *frame, void *context)
{
    static int const gray[6] = {0, 8192, 16384, 32768, 49152, 65535};
    static int const coverage[6] = {65535, 65535, 16384, 32768, 49151, 65535};
    output_bpc_context_t *test;
    unsigned int history;
    float const *rgb;
    double expected;
    double value;
    double alpha;
    int pixel;
    int channel;

    test = context;
    ++test->calls;
    history = SIXEL_FRAME_CMS_EMBEDDED_ICC;
    if (test->enabled) {
        history |= SIXEL_FRAME_CMS_OUTPUT_BPC;
    }
    if (sixel_frame_get_pixelformat(frame) != SIXEL_PIXELFORMAT_LINEARRGBFLOAT32
        || frame->cms_history != history
        || sixel_frame_get_width(frame) != 6
        || sixel_frame_get_height(frame) != 1) {
        return SIXEL_BAD_INPUT;
    }
    rgb = (float const *)sixel_frame_get_pixels(frame);
    for (pixel = 0; pixel < 6; ++pixel) {
        alpha = coverage[pixel] / 65535.0;
        /* The source encodes Y=(1+3x)/4. Mapping black 1/4 to 1/2
         * gives (1+x)/2, including the file's bKGD word 4096. The
         * declared fixed-point D50 chromaticity allows a small RGB error. */
        value = (alpha * gray[pixel] + (1.0 - alpha) * 4096.0) / 65535.0;
        expected = test->enabled ? (1.0 + value) / 2.0
                                 : (1.0 + 3.0 * value) / 4.0;
        if (test->geometry && pixel == 3) {
            return output_bpc_check_geometry(frame, history, expected)
                ? SIXEL_OK : SIXEL_BAD_INPUT;
        }
        for (channel = 0; channel < 3; ++channel) {
            if (!(fabs(rgb[pixel * 3 + channel] - expected) < 3.0e-5)) {
                fprintf(stderr, "pixel %d channel %d: %.9g vs %.9g\n",
                        pixel, channel, rgb[pixel * 3 + channel], expected);
                return SIXEL_BAD_INPUT;
            }
        }
    }
    return SIXEL_OK;
}

int
test_output_bpc_float(int argc, char **argv)
{
    sixel_loader_t *loader;
    SIXELSTATUS status;
    output_bpc_context_t test;

    if (argc != 3 && (argc != 4 || (strcmp(argv[3], "off") != 0 &&
                                   strcmp(argv[3], "geometry") != 0))) {
        return 1;
    }
    loader = NULL;
    test.geometry = argc == 4 && strcmp(argv[3], "geometry") == 0;
    test.enabled = argc == 3 || test.geometry;
    test.calls = 0;
    status = sixel_loader_new(&loader, NULL);
    if (SIXEL_FAILED(status)) {
        return 1;
    }
    if (test.enabled) {
        sixel_loader_set_output_bpc(loader, 0.5);
    }
    status = sixel_loader_setopt(loader, SIXEL_LOADER_OPTION_LOADER_ORDER,
                                  argv[1]);
    if (SIXEL_SUCCEEDED(status)) {
        status = sixel_loader_setopt(loader, SIXEL_LOADER_OPTION_CONTEXT,
                                      &test);
    }
    if (SIXEL_SUCCEEDED(status)) {
        status = sixel_loader_load_file(loader, argv[2],
                                        output_bpc_check_float);
    }
    if (SIXEL_FAILED(status)) {
        fprintf(stderr, "%s\n", sixel_helper_get_additional_message());
    }
    sixel_loader_unref(loader);
    return SIXEL_FAILED(status) || test.calls != 1;
}
