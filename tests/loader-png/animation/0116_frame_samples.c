/* SPDX-License-Identifier: MIT */
/* Check source-derived opaque RGB and every transparency-mask position. */
#include "config.h"
#include <math.h>
#include <stdio.h>
#include "src/loader.h"
#include "src/frame-private.h"
#include "tests/data/color-numeric/png-keycolor.h"

#if HAVE_LIBPNG
static SIXELSTATUS
png_key_frame(sixel_frame_t *frame, void *context)
{
    float const *pixels;
    int *calls;
    unsigned char const *rgb;
    double actual;
    int i;
    int c;
    int hidden;

    calls = context;
    ++*calls;
    if (frame->width != 32 || frame->height != 32 ||
        frame->pixelformat != SIXEL_PIXELFORMAT_LINEARRGBFLOAT32 ||
        frame->colorspace != SIXEL_COLORSPACE_LINEAR ||
        frame->transparent_mask_size != 1024 || !frame->transparent_mask) {
        return SIXEL_BAD_INPUT;
    }
    pixels = (float const *)sixel_frame_get_pixels(frame);
    for (i = 0; i < 1024; ++i) {
        rgb = color_key_rgb[i];
        hidden = rgb[0] == 255 && rgb[1] == 255 && rgb[2] == 255;
        if (!!frame->transparent_mask[i] != hidden) {
            fprintf(stderr, "keycolor source mask mismatch at %d\n", i);
            return SIXEL_BAD_INPUT;
        }
        if (hidden) {
            continue;
        }
        for (c = 0; c < 3; ++c) {
            actual = pixels[i * 3 + c];
            if (!isfinite(actual) || actual < color_key_linear[rgb[c]][0] ||
                actual > color_key_linear[rgb[c]][1]) {
                fprintf(stderr, "keycolor RGB mismatch at %d:%d\n", i, c);
                return SIXEL_BAD_INPUT;
            }
        }
    }
    return SIXEL_OK;
}
#endif

int
test_png_key_0001(int argc, char **argv)
{
#if HAVE_LIBPNG
    sixel_loader_t *loader;
    SIXELSTATUS status;
    int no;
    int calls;
    int mode;

    if (argc != 2) {
        return 1;
    }
    no = 0;
    for (mode = 0; mode < 2; ++mode) {
        loader = NULL;
        calls = 0;
        status = sixel_loader_new(&loader, NULL);
        if (SIXEL_SUCCEEDED(status)) {
            status = sixel_loader_setopt(loader, SIXEL_LOADER_OPTION_CONTEXT,
                                          &calls);
        }
        if (SIXEL_SUCCEEDED(status)) {
            status = sixel_loader_setopt(loader,
                SIXEL_LOADER_OPTION_USE_PALETTE, &no);
        }
        if (SIXEL_SUCCEEDED(status)) {
            status = sixel_loader_setopt(loader,
                SIXEL_LOADER_OPTION_LOADER_ORDER,
                mode ? "libpng:K1:cms_engine=none!" :
                       "libpng:K0:cms_engine=none!");
        }
        if (SIXEL_SUCCEEDED(status)) {
            status = sixel_loader_load_file(loader, argv[1], png_key_frame);
        }
        sixel_loader_unref(loader);
        if (SIXEL_FAILED(status) || calls != 1) {
            return 1;
        }
    }
    return 0;
#else
    (void)argc;
    (void)argv;
    return 77;
#endif
}
