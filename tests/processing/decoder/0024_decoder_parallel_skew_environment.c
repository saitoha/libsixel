/*
 * SPDX-License-Identifier: MIT
 *
 * Verify the decoder parallel skew environment reaches span planning.
 */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <stdlib.h>

#include "src/compat_stub.h"
#include "src/decoder-parallel.h"

int
test_decoder_0024_decoder_parallel_skew_environment(int argc, char **argv)
{
    int value;

    (void)argc;
    (void)argv;

    if (sixel_compat_setenv("SIXEL_PARALLEL_SKEW", "10") != 0) {
        return EXIT_FAILURE;
    }
    value = sixel_decoder_parallel_skew_percent();
    (void)sixel_compat_setenv("SIXEL_PARALLEL_SKEW", "");
    if (value != 10) {
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
