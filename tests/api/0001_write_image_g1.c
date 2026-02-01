/*
 * Verify sixel_helper_write_image_file() rejects a G1 pngsuite sample
 * without implicit pixelformat normalization.
 */

#include "tests/api/api_test_common.h"

int
test_api_0001_write_image_g1(int argc, char **argv)
{
    (void) argc;
    (void) argv;

    return run_write_image_case_expect_failure("api write g1",
                                               PNGSUITE_G1_PATH,
                                               SIXEL_PIXELFORMAT_G1,
                                               SIXEL_BAD_ARGUMENT);
}

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
