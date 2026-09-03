/*
 * SPDX-License-Identifier: MIT
 *
 * Verify the scale threshold environment reaches parallel dispatch.
 */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <stdlib.h>

#include "src/compat_stub.h"
#include "src/scale.h"

int
test_scale_0002_parallel_min_bytes_environment(int argc, char **argv)
{
    size_t value;

    (void)argc;
    (void)argv;

    if (sixel_compat_setenv("SIXEL_SCALE_PARALLEL_MIN_BYTES", "17") != 0) {
        return EXIT_FAILURE;
    }
    value = sixel_scale_parallel_min_bytes();
    (void)sixel_compat_setenv("SIXEL_SCALE_PARALLEL_MIN_BYTES", "");
    if (value != 17u) {
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
