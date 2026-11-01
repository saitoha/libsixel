/*
 * SPDX-License-Identifier: MIT
 *
 * Verify the scale parallel factor environment reaches band planning.
 */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <stdlib.h>
#include <string.h>

#include "src/compat_stub.h"
#include "src/scale.h"
#include "src/options.h"

int
test_scale_0001_parallel_factor_environment(int argc, char **argv)
{
    SIXELSTATUS status;
    char diagnostic[128];
    int value;

    status = SIXEL_OK;
    diagnostic[0] = '\0';
    if (argc > 1 && strcmp(argv[1], "cli") == 0) {
        status = sixel_option_apply_runtime_policy_argument(
            "auto:F3",
            SIXEL_OPTION_SCOPE_ENCODER,
            diagnostic,
            sizeof(diagnostic));
        if (SIXEL_FAILED(status)) {
            return EXIT_FAILURE;
        }
    } else {
        if (sixel_compat_setenv("SIXEL_PARALLEL_FACTOR", "3") != 0) {
            return EXIT_FAILURE;
        }
    }
    value = sixel_scale_parallel_band_span(10, 2);
    (void)sixel_compat_setenv("SIXEL_PARALLEL_FACTOR", "");
    if (value != 3) {
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
