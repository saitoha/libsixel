/*
 * SPDX-License-Identifier: MIT
 *
 * Lock k_undither output when edge protection is enabled.
 */

#include "processing/decoder/kundither_test_common.h"

static unsigned char const g_expected_edge[] = {
    11, 23, 5, 48, 90, 102, 70, 160, 172, 106, 172, 72,
    130, 87, 142, 196, 102, 234, 233, 169, 75, 14, 236, 172,
    50, 65, 49, 88, 114, 110, 125, 181, 207, 162, 248, 48,
    208, 90, 112, 236, 126, 242, 31, 177, 108, 54, 4, 180,
    92, 92, 62, 128, 138, 118, 165, 205, 215, 203, 30, 82,
    233, 94, 181, 20, 150, 250, 72, 187, 101, 107, 78, 139
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
