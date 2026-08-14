/*
 * SPDX-License-Identifier: MIT
 *
 * Verify that the GPU blue-noise path compares retained RGB against the
 * palette candidate selected from the jittered sample.  The original source
 * color must remain the fidelity reference after blue-noise perturbation.
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
test_filter_0025_filter_dither_6delta_bluenoise_gpu(int argc, char **argv)
{
    SIXELSTATUS status;
    sixel_gpu_palette_request_t request;
    sixel_index_t dest[1];
    unsigned char palette[6];
    unsigned char pixel[3];
    unsigned char retained[3];
    unsigned char valid_mask[1];
    unsigned char result_mask[1];
    int ok;

    (void)argc;
    (void)argv;
    status = SIXEL_FALSE;
    memset(&request, 0, sizeof(request));
    dest[0] = 255u;
    palette[0] = 0u;
    palette[1] = 0u;
    palette[2] = 0u;
    palette[3] = 255u;
    palette[4] = 255u;
    palette[5] = 255u;
    pixel[0] = 140u;
    pixel[1] = 140u;
    pixel[2] = 140u;
    retained[0] = 255u;
    retained[1] = 255u;
    retained[2] = 255u;
    valid_mask[0] = 1u;
    result_mask[0] = 0u;
    ok = 0;

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
    request.sixdelta_enabled = 1;
    request.has_6delta_accumulation = 1;
    request.accumulation_pixels = retained;
    request.accumulation_pixels_size = sizeof(retained);
    request.accumulation_valid_mask = valid_mask;
    request.accumulation_valid_mask_size = sizeof(valid_mask);
    request.accumulation_keycolor = 7;
    request.sixdelta_threshold = 0U;
    request.accumulation_result_mask = result_mask;
    request.accumulation_result_mask_size = sizeof(result_mask);

    status = sixel_gpu_palette_apply(&request);
    if (SIXEL_FAILED(status)) {
        fprintf(stderr, "GPU blue-noise palette apply failed: %04x\n", status);
        goto end;
    }
    if (dest[0] != 7u || result_mask[0] != 1u) {
        fprintf(stderr,
                "GPU blue-noise keep mismatch: index=%u mask=%u\n",
                (unsigned int)dest[0],
                (unsigned int)result_mask[0]);
        goto end;
    }
    ok = 1;

end:
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
