/* SPDX-License-Identifier: MIT */
/* Verify builtin matrix fallback against its numeric contract. */
#include "tests/_support/loader/png_cicp_test_common.h"

int
test_cicp_0019(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return png_cicp_test(19, 0);
}

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
