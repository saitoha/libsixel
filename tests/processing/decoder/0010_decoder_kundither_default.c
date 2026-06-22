/*
 * SPDX-License-Identifier: MIT
 *
 * Lock the default k_undither RGB output for a 32-color indexed image.
 */

#include "processing/decoder/kundither_test_common.h"

static unsigned char const g_expected_default[] = {
    23, 45, 37, 44, 98, 86, 70, 153, 141, 108, 178, 98,
    135, 96, 155, 160, 120, 203, 223, 126, 134, 34, 183, 137,
    51, 83, 86, 88, 114, 110, 138, 129, 157, 139, 186, 84,
    206, 95, 131, 214, 132, 180, 38, 160, 124, 63, 75, 139,
    93, 115, 90, 108, 156, 120, 200, 144, 211, 208, 46, 111,
    230, 86, 169, 31, 134, 179, 85, 155, 102, 104, 70, 142
};

int
test_decoder_0010_decoder_kundither_default(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    return kundither_test_run("kundither default", 100, 0,
                              g_expected_default);
}

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
