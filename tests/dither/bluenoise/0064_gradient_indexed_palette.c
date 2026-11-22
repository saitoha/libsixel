/* SPDX-License-Identifier: MIT */
#include <stdlib.h>
#include <string.h>
#include <sixel.h>

#include "dither.h"
#include "filter-factory.h"
#include "filter-gradient.h"

int
test_dither_bluenoise_0064(int argc, char **argv)
{
    unsigned char pixels[] = {0, 2, 1};
    unsigned char palette[] = {0, 0, 0, 255, 255, 255, 0, 0, 0};
    /* The first two indices are both black. At the black-to-white edge,
     * the clamped Sobel kernel has magnitude 4 * 255, which scales to 180. */
    unsigned char const expected[] = {0, 180, 180};
    sixel_allocator_t *allocator;
    sixel_dither_t *dither;
    sixel_frame_t *frame;
    sixel_filter_t *filter;
    sixel_filter_gradient_config_t config;
    int result;

    (void)argc;
    (void)argv;
    allocator = NULL;
    dither = NULL;
    frame = NULL;
    filter = NULL;
    result = EXIT_FAILURE;

    if (SIXEL_FAILED(sixel_allocator_new(&allocator, NULL, NULL, NULL, NULL)) ||
        SIXEL_FAILED(sixel_dither_new(&dither, 3, allocator)) ||
        SIXEL_FAILED(sixel_frame_new(&frame, allocator)) ||
        SIXEL_FAILED(sixel_frame_init_borrowed(frame, pixels, 3, 1,
                     SIXEL_PIXELFORMAT_PAL8, palette, 3))) {
        goto end;
    }

    sixel_dither_set_diffusion_type(dither, SIXEL_DIFFUSE_BLUENOISE_DITHER);
    dither->bluenoise_gradient_factor_override = 1;
    dither->bluenoise_gradient_factor = 1.0f;
    config.dither = dither;
    if (SIXEL_FAILED(sixel_filter_factory_create_by_kind(
            SIXEL_FILTER_KIND_GRADIENT, &config, &filter))) {
        goto end;
    }
    sixel_filter_bind_input(filter, &frame, SIXEL_PIXELFORMAT_PAL8,
                            SIXEL_COLORSPACE_GAMMA);
    if (sixel_filter_run(filter, allocator, NULL) == SIXEL_OK &&
        dither->bluenoise_gradient_map != NULL &&
        dither->bluenoise_gradient_map_size == sizeof(expected) &&
        memcmp(dither->bluenoise_gradient_map, expected,
               sizeof(expected)) == 0) {
        result = EXIT_SUCCESS;
    }

end:
    sixel_filter_free(filter);
    sixel_frame_unref(frame);
    sixel_dither_unref(dither);
    sixel_allocator_unref(allocator);
    return result;
}
