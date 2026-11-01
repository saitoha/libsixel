/* SPDX-License-Identifier: MIT */
/* Distinct means, pixel means and extrema have independent literal answers.
 * The float view detects an accidental byte projection as well as resolution.
 */
#include <math.h>
#include <sixel.h>
#include "src/dither.h"
#include "src/palette.h"
#include "src/palette-common-merge.h"

int
heckbert_defaults_check(int diffusion, int representative, int ward,
                        int expected)
{
    float pixels[18] = {
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        .25f, .25f, .25f, 1, 1, 1
    };
    sixel_dither_t *dither;
    sixel_palette_float32_entries_view_t view;
    SIXELSTATUS status;
    double value;
    int ok;

    dither = NULL;
    ok = 0;
    value = expected == SIXEL_REP_CENTER_BOX ? .5 :
        expected == SIXEL_REP_AVERAGE_COLORS ? 5.0 / 12.0 : 5.0 / 24.0;
    status = sixel_dither_new(&dither, 1, NULL);
    if (SIXEL_FAILED(status)) {
        return 1;
    }
    if (diffusion >= 0) {
        sixel_dither_set_diffusion_type(dither, diffusion);
    }
    sixel_dither_set_final_merge(dither, ward ? SIXEL_FINAL_MERGE_WARD :
        SIXEL_FINAL_MERGE_NONE);
    sixel_set_final_merge_target_factor_override(1, 3.0);
    sixel_set_final_merge_lloyd_iterations_override(1, 3U);
    status = sixel_dither_initialize(dither, (unsigned char *)pixels, 6, 1,
        SIXEL_PIXELFORMAT_RGBFLOAT32, SIXEL_LARGE_AUTO, representative,
        SIXEL_QUALITY_FULL);
    if (SIXEL_SUCCEEDED(status)) {
        status = dither->palette->vtbl->get_entries_float32(dither->palette,
            &view);
        ok = SIXEL_SUCCEEDED(status) && view.entry_count == 1 &&
            fabs(view.entries[0] - value) < 1e-7 &&
            fabs(view.entries[1] - value) < 1e-7 &&
            fabs(view.entries[2] - value) < 1e-7 &&
            dither->method_for_rep == expected;
    }
    sixel_set_final_merge_target_factor_override(0, 1.81);
    sixel_set_final_merge_lloyd_iterations_override(0, 3U);
    sixel_dither_unref(dither);
    return ok ? 0 : 1;
}
