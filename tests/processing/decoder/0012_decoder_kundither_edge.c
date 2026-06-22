/*
 * SPDX-License-Identifier: MIT
 *
 * Lock k_undither output when edge protection is enabled.
 */

#include "processing/decoder/kundither_test_common.h"

static unsigned char const g_expected_edge[] = {
    18, 36, 24, 46, 88, 86, 85, 157, 199, 122, 224, 40,
    159, 35, 137, 196, 102, 234, 233, 169, 75, 14, 236, 172,
    51, 47, 13, 88, 114, 110, 125, 181, 207, 162, 248, 48,
    199, 59, 145, 236, 126, 242, 17, 193, 83, 54, 4, 180,
    91, 71, 21, 128, 138, 118, 165, 205, 215, 202, 16, 56,
    239, 83, 153, 20, 150, 250, 57, 217, 91, 107, 78, 139
};

int
test_decoder_0012_decoder_kundither_edge(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    return kundither_test_run("kundither edge", 120, 25,
                              g_expected_edge);
}

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
