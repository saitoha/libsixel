/* SPDX-License-Identifier: MIT */
/* Verify GPU 6delta indices and accumulation masks. */
#include <sixel.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "src/gpu-palette.h"

#define POS_GPU_WIDTH 4
#define POS_GPU_HEIGHT 2
#define POS_GPU_PIXELS (POS_GPU_WIDTH * POS_GPU_HEIGHT)
#define POS_GPU_KEYCOLOR 7

static int
pos_gpu_expect_byte(char const *name, unsigned char actual,
                    unsigned char expected)
{
    if (actual == expected) {
        return 1;
    }
    fprintf(stderr,
            "%s: expected %u, got %u\n",
            name,
            (unsigned int)expected,
            (unsigned int)actual);
    return 0;
}

int
test_positional_0048_gpu_6delta(int argc, char **argv)
{
    SIXELSTATUS status;
    sixel_gpu_palette_request_t request;
    sixel_index_t dest[POS_GPU_PIXELS];
    unsigned char transparent_mask[POS_GPU_PIXELS];
    unsigned char result_mask[POS_GPU_PIXELS];
    unsigned char valid_mask[POS_GPU_PIXELS];
    unsigned char pixels[POS_GPU_PIXELS * 3];
    unsigned char accumulation[POS_GPU_PIXELS * 3];
    unsigned char palette[8 * 3];
    int ok;

    (void)argc;
    (void)argv;
    status = SIXEL_FALSE;
    memset(&request, 0, sizeof(request));
    memset(dest, 0xff, sizeof(dest));
    memset(transparent_mask, 0, sizeof(transparent_mask));
    memset(result_mask, 0, sizeof(result_mask));
    memset(valid_mask, 1, sizeof(valid_mask));
    memset(pixels, 0, sizeof(pixels));
    memset(accumulation, 0, sizeof(accumulation));
    memset(palette, 0, sizeof(palette));
    ok = 1;

    palette[1 * 3 + 0] = 255;
    palette[2 * 3 + 1] = 255;
    palette[3 * 3 + 2] = 255;
    palette[4 * 3 + 0] = 255;
    palette[4 * 3 + 1] = 255;
    palette[4 * 3 + 2] = 255;
    palette[POS_GPU_KEYCOLOR * 3 + 0] = 128;
    palette[POS_GPU_KEYCOLOR * 3 + 1] = 128;
    palette[POS_GPU_KEYCOLOR * 3 + 2] = 128;

    pixels[0] = 10;
    pixels[1] = 10;
    pixels[2] = 10;
    accumulation[0] = 12;
    accumulation[1] = 9;
    accumulation[2] = 10;

    pixels[3] = 255;
    pixels[4] = 0;
    pixels[5] = 0;

    pixels[6] = 0;
    pixels[7] = 255;
    pixels[8] = 0;
    transparent_mask[2] = 1;
    result_mask[2] = 1;

    pixels[9] = 10;
    pixels[10] = 10;
    pixels[11] = 10;
    accumulation[9] = 10;
    accumulation[10] = 10;
    accumulation[11] = 10;
    valid_mask[3] = 0;

    request.policy = SIXEL_GPU_POLICY_FORCE;
    request.dest = dest;
    request.pixels = pixels;
    request.pixel_count = POS_GPU_PIXELS;
    request.width = POS_GPU_WIDTH;
    request.height = POS_GPU_HEIGHT;
    request.pixelformat = SIXEL_PIXELFORMAT_RGB888;
    request.palette = palette;
    request.palette_size = sizeof(palette);
    request.palette_depth = 3;
    request.ncolors = 8;
    request.lut_policy = SIXEL_LUT_POLICY_NONE;
    request.method_for_diffuse = SIXEL_DIFFUSE_NONE;
    request.method_for_scan = SIXEL_SCAN_RASTER;
    request.transparent_mask = transparent_mask;
    request.transparent_mask_size = sizeof(transparent_mask);
    request.transparent_keycolor = POS_GPU_KEYCOLOR;
    request.sixdelta_enabled = 1;
    request.has_6delta_accumulation = 1;
    request.accumulation_pixels = accumulation;
    request.accumulation_pixels_size = sizeof(accumulation);
    request.accumulation_valid_mask = valid_mask;
    request.accumulation_valid_mask_size = sizeof(valid_mask);
    request.accumulation_keycolor = POS_GPU_KEYCOLOR;
    request.sixdelta_threshold = 3;
    request.accumulation_result_mask = result_mask;
    request.accumulation_result_mask_size = sizeof(result_mask);

    status = sixel_gpu_palette_apply(&request);
    if (SIXEL_FAILED(status)) {
        fprintf(stderr, "gpu palette apply failed: %04x\n", status);
        return EXIT_FAILURE;
    }

    ok &= pos_gpu_expect_byte("kept index", dest[0], POS_GPU_KEYCOLOR);
    /* The nonexact early keep carries its refinement reason bit. */
    ok &= pos_gpu_expect_byte("kept mask", result_mask[0], 3);
    ok &= pos_gpu_expect_byte("painted index", dest[1], 1);
    ok &= pos_gpu_expect_byte("painted mask", result_mask[1], 0);
    ok &= pos_gpu_expect_byte("transparent index", dest[2], POS_GPU_KEYCOLOR);
    ok &= pos_gpu_expect_byte("transparent mask", result_mask[2], 1);
    ok &= pos_gpu_expect_byte("invalid accumulation index", dest[3], 0);
    ok &= pos_gpu_expect_byte("invalid accumulation mask", result_mask[3], 0);

    return ok != 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
