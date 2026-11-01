/* SPDX-License-Identifier: MIT */
/* Heckbert AUTO exports signed distinct-color means after Ward refinement. */
#include "../../_support/quant/palette/ordinary_float/ordinary_float_test_common.h"

int
test_ordinary_heckbert_polish_auto_average(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    /* A palette-only caller has no diffusion state, so AUTO uses the
     * distinct-color mean (-.5 - .25 + 1.5) / 3, including after Ward. */
    return ordinary_heckbert_representative(SIXEL_REP_AUTO, 1) ? 0 : 1;
}
