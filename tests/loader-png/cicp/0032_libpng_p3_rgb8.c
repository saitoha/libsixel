/* SPDX-License-Identifier: MIT */
/* Verify libpng p3 rgb8 against its numeric contract. */
#include "tests/_support/loader/png_cicp_test_common.h"

int
test_cicp_0032(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return png_cicp_test(1, 1);
}

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
