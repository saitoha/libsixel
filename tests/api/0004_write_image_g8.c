/*
 * Verify sixel_helper_write_image_file() accepts a G8 pngsuite sample.
 */

#include "tests/api/api_test_common.h"

int
test_api_0004_write_image_g8(int argc, char **argv)
{
    (void) argc;
    (void) argv;

    return run_write_image_case("api write g8",
                                PNGSUITE_G8_PATH,
                                SIXEL_PIXELFORMAT_G8);
}

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
