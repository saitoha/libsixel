/* SPDX-License-Identifier: MIT */
#include <stdlib.h>
#include <stdio.h>
#include "palette_stage2_common.h"
#include "src/cms.h"

int
test_palette_stage2_0027(int argc, char **argv)
{
    sixel_palette_build_request_t *request;
    sixel_palette_snapshot_t *snapshot;
    sixel_palette_snapshot_profile_t const *profile;
    sixel_palette_frame_state_t state;
    sixel_frame_t *frame;
    sixel_dither_t *built;
    sixel_dither_t *copy;
    int failed;
    SIXELSTATUS status;

    (void)argc;
    (void)argv;
    snapshot = NULL;
    built = copy = NULL;
    failed = 1;
    if (palette_stage2_request(NULL, 1, SIXEL_LUT_POLICY_FHEDT, 1, 1,
            &request, &frame)) {
        return 1;
    }
    status = sixel_palette_build_request_execute(request, frame,
                                                  &built, &state, NULL);
    if (SIXEL_SUCCEEDED(status)) {
        status = sixel_palette_snapshot_take(&snapshot, request,
                                              &built, &state);
    }
    if (SIXEL_FAILED(status) || built != NULL) {
        fprintf(stderr, "owned request build/take: %x %s\n", status,
            sixel_helper_get_additional_message());
        goto end;
    }
    profile = sixel_palette_snapshot_profile(snapshot);
    if (profile->consumer_pixelformat != SIXEL_PIXELFORMAT_RGBFLOAT32 ||
            profile->working_colorspace != SIXEL_COLORSPACE_GAMMA ||
            profile->delta_alpha_key != 1 || profile->keycolor_reserved != 1 ||
            profile->paintable_colors != 2 || profile->output_icc == NULL ||
            profile->construction.quantizer.phase !=
                SIXEL_PALETTE_POLICY_EXECUTED) {
        fprintf(stderr, "profile format=%x work=%d delta=%d key=%d "
            "paint=%d icc=%p phase=%d\n", profile->consumer_pixelformat,
            profile->working_colorspace, profile->delta_alpha_key,
            profile->keycolor_reserved, profile->paintable_colors,
            (void *)profile->output_icc, profile->construction.quantizer.phase);
        goto end;
    }
    sixel_palette_build_request_free(request);
    request = NULL;
    if (SIXEL_FAILED(sixel_palette_snapshot_clone(snapshot, &copy)) ||
            copy->ncolors != 3 || copy->keycolor != 2 ||
            copy->lookup_policy == NULL) {
        fprintf(stderr, "owned snapshot clone: %s\n",
            sixel_helper_get_additional_message());
        goto end;
    }
    failed = 0;
end:
    sixel_palette_snapshot_unref(snapshot);
    sixel_dither_unref(copy);
    sixel_dither_unref(built);
    sixel_frame_unref(frame);
    sixel_palette_build_request_free(request);
    return failed;
}
