/*
 * SPDX-License-Identifier: MIT
 *
 * Lock k_undither output when the palette similarity bias changes.
 */

#include "processing/decoder/kundither_test_common.h"

static unsigned char const g_expected_similarity[] = {
    11, 23, 5, 49, 75, 72, 59, 177, 170, 122, 224, 40,
    135, 61, 128, 172, 128, 225, 233, 169, 75, 40, 197, 163,
    50, 61, 42, 88, 114, 110, 143, 150, 191, 162, 248, 48,
    200, 44, 115, 219, 135, 213, 34, 184, 112, 54, 4, 180,
    90, 85, 50, 109, 168, 133, 188, 178, 224, 201, 30, 85,
    238, 97, 182, 20, 150, 250, 80, 190, 100, 94, 28, 188
};

int
test_decoder_0011_decoder_kundither_similarity(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    return kundither_test_run("kundither similarity", 240, 0,
                              g_expected_similarity);
}

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
