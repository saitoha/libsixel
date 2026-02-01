/*
 * Verify sixel_helper_write_image_file() accepts a PAL1 pngsuite sample.
 */

#include "tests/api/api_test_common.h"

int
test_api_0005_write_image_pal1(int argc, char **argv)
{
    (void) argc;
    (void) argv;

    return run_write_image_case("api write pal1",
                                PNGSUITE_PAL1_PATH,
                                SIXEL_PIXELFORMAT_PAL1);
}

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
