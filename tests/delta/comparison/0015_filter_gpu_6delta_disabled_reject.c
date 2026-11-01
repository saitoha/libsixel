/*
 * SPDX-License-Identifier: MIT
 *
 * Reject a contradictory GPU request that provides retained RGB while the
 * caller explicitly disables 6delta keeps.
 */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sixel.h>

#include "src/gpu-palette.h"

int
test_filter_0027_filter_gpu_6delta_disabled_reject(int argc, char **argv)
{
    SIXELSTATUS status;
    sixel_gpu_palette_request_t request;
    sixel_index_t dest[1];
    unsigned char palette[6];
    unsigned char pixel[3];
    unsigned char retained[3];
    unsigned char valid_mask[1];

    (void)argc;
    (void)argv;
    status = SIXEL_FALSE;
    memset(&request, 0, sizeof(request));
    memset(dest, 0, sizeof(dest));
    memset(palette, 0, sizeof(palette));
    memset(pixel, 0, sizeof(pixel));
    memset(retained, 0, sizeof(retained));
    valid_mask[0] = 1u;

    request.policy = SIXEL_GPU_POLICY_FORCE;
    request.dest = dest;
    request.pixels = pixel;
    request.pixel_count = 1U;
    request.width = 1;
    request.height = 1;
    request.pixelformat = SIXEL_PIXELFORMAT_RGB888;
    request.palette = palette;
    request.palette_size = sizeof(palette);
    request.palette_depth = 3;
    request.ncolors = 2;
    request.lut_policy = SIXEL_LUT_POLICY_NONE;
    request.method_for_diffuse = SIXEL_DIFFUSE_BLUENOISE_DITHER;
    request.method_for_scan = SIXEL_SCAN_RASTER;
    request.sixdelta_enabled = 0;
    request.has_6delta_accumulation = 1;
    request.accumulation_pixels = retained;
    request.accumulation_pixels_size = sizeof(retained);
    request.accumulation_valid_mask = valid_mask;
    request.accumulation_valid_mask_size = sizeof(valid_mask);
    request.accumulation_keycolor = 7;
    request.sixdelta_threshold = 0U;

    status = sixel_gpu_palette_apply(&request);
    if (status != SIXEL_BAD_ARGUMENT) {
        fprintf(stderr,
                "contradictory GPU request was not rejected: %04x\n",
                status);
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
