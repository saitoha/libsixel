/* SPDX-License-Identifier: MIT */
/* Verify literal TIFF words at the public frame boundary.
 * Expected values are independent of the selected CMM. */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "src/cms.h"
#include "src/frame-private.h"
#include "src/loader.h"
#include "src/loader-manager.h"
#include "tests/data/color-numeric/tiff-rgb16.h"

typedef struct {
    int fixture;
    int managed;
    int calls;
    double relative;
} tiff_numeric_t;

static SIXELSTATUS
tiff_numeric_capture(sixel_frame_t *frame, void *context)
{
    tiff_numeric_t *probe;
    float const *pixels;
    double expected;
    double scale;
    uint32_t bits;
    int format;
    int space;
    int fixture;
    int source;
    int linear;
    int orientation;
    int x;
    int y;
    int index;
    int p;
    int c;

    probe = (tiff_numeric_t *)context;
    ++probe->calls;
    fixture = probe->fixture;
    source = color_tiff_cases[fixture].source;
    linear = color_tiff_cases[fixture].linear;
    orientation = color_tiff_cases[fixture].orientation;
    format = probe->managed ? SIXEL_PIXELFORMAT_LINEARRGBFLOAT32
                            : SIXEL_PIXELFORMAT_RGBFLOAT32;
    space = probe->managed ? SIXEL_COLORSPACE_LINEAR : SIXEL_COLORSPACE_GAMMA;
    if (frame->width != color_tiff_cases[fixture].width ||
        frame->height != color_tiff_cases[fixture].height ||
        sixel_frame_get_pixelformat(frame) != format ||
        frame->colorspace != space || frame->transparent_mask != NULL) {
        fprintf(stderr, "TIFF frame %dx%d format=%x space=%d\n",
            frame->width, frame->height, sixel_frame_get_pixelformat(frame),
            frame->colorspace);
        return SIXEL_BAD_INPUT;
    }
    pixels = (float const *)sixel_frame_get_pixels(frame);
    for (p = 0; p < COLOR_TIFF_SAMPLES; ++p) {
        /* Invert TIFF orientation using coordinates in the original raster.
         * The oracle does not call the adapter's scanline mapping function. */
        x = p % frame->width;
        y = p / frame->width;
        if (orientation == 2 || orientation == 3) {
            x = frame->width - 1 - x;
        }
        if (orientation == 3 || orientation == 4) {
            y = frame->height - 1 - y;
        }
        index = y * frame->width + x;
        scale = 0.0;
        for (c = 0; c < 3; ++c) {
            expected = color_tiff_linear[linear][index][c];
            if (fabs(expected) > scale) {
                scale = fabs(expected);
            }
        }
        for (c = 0; c < 3; ++c) {
            if (!probe->managed) {
                /* Failed or absent CMS must publish original normalized
                 * source bits and the GAMMA device-coordinate marker. */
                memcpy(&bits, pixels + p * 3 + c, sizeof(bits));
                if (bits != color_tiff_device[source][index][c]) {
                    fprintf(stderr, "TIFF source pixel=%d channel=%d\n",
                        p, c);
                    return SIXEL_BAD_INPUT;
                }
                continue;
            }
            expected = color_tiff_linear[linear][index][c];
            /* Reuse the existing CMS triplet-magnitude budget. Matrix
             * cross-talk is permitted, but there is no absolute dark floor:
             * black is exact zero and dark positive samples cannot vanish. */
            if (!isfinite(pixels[p * 3 + c]) ||
                fabs(pixels[p * 3 + c] - expected) >
                    scale * probe->relative) {
                fprintf(stderr, "TIFF pixel=%d channel=%d %.17g != %.17g\n",
                    p, c, pixels[p * 3 + c], expected);
                return SIXEL_BAD_INPUT;
            }
        }
    }
    return SIXEL_OK;
}

int
test_tiff_0142(int argc, char **argv)
{
    tiff_numeric_t probe;
    sixel_cms_engine_t engine;
    sixel_cms_engine_t previous;
    sixel_loader_t *loader;
    SIXELSTATUS status;
    char order[160];
    int mode;
    int no;
    int length;
    int ok;

    if (argc != 5 || !sixel_cms_engine_from_string(argv[1], &engine)) {
        return 1;
    }
    memset(&probe, 0, sizeof(probe));
    probe.fixture = atoi(argv[2]);
    mode = atoi(argv[3]);
    if (probe.fixture < 0 || probe.fixture >= COLOR_TIFF_FIXTURES ||
        mode < 0 || mode > 2 ||
        (mode == 2 && engine != SIXEL_CMS_ENGINE_COLORSYNC)) {
        return 1;
    }
    if (!loader_manager_entry_available("libtiff")) {
        return 77;
    }
    previous = sixel_cms_get_engine();
    sixel_cms_set_engine(engine);
    if (sixel_cms_get_engine() != engine) {
        sixel_cms_set_engine(previous);
        return 77;
    }
    probe.managed = mode == 0;
    probe.relative = engine == SIXEL_CMS_ENGINE_COLORSYNC
        ? color_tiff_native_relative : color_tiff_float_relative;
    loader = NULL;
    no = 0;
    ok = 0;
    length = snprintf(order, sizeof(order),
        "libtiff:cms_engine=%s:cms_target=linear:prefer_8bit=0!", argv[1]);
    if (length < 0 || (size_t)length >= sizeof(order)) {
        goto end;
    }
    status = sixel_loader_new(&loader, NULL);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    status = sixel_loader_setopt(loader, SIXEL_LOADER_OPTION_CONTEXT,
                                 &probe);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    sixel_loader_set_prefer_float32(loader, 1);
    status = sixel_loader_setopt(loader, SIXEL_LOADER_OPTION_USE_PALETTE, &no);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    status = sixel_loader_setopt(loader, SIXEL_LOADER_OPTION_LOADER_ORDER,
                                 order);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    sixel_helper_set_additional_message("");
    status = sixel_loader_load_file(loader, argv[4], tiff_numeric_capture);
    ok = SIXEL_SUCCEEDED(status) && probe.calls == 1 &&
        sixel_cms_get_engine() == engine;
    if (ok && mode == 2) {
        /* This diagnostic is produced only after a ColorSync source reaches
         * preparation. Disabling CMS cannot satisfy this failure control.
         * It is not a simulation of a native apply that wrote a prefix. */
        ok = strcmp(sixel_helper_get_additional_message(),
            "SIXEL_LOADER_CMS_COLORSYNC_NATIVE_GAMMA must be 0 or 1.") == 0;
        if (ok) {
            fprintf(stderr, "TIFFNUM|engine=colorsync|prepare-failed\n");
        }
    }

end:
    sixel_loader_unref(loader);
    sixel_cms_set_engine(previous);
    return ok ? 0 : 1;
}
