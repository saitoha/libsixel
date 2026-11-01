/*
 * SPDX-License-Identifier: MIT
 *
 * Verify the scale threshold environment reaches parallel dispatch.
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
test_scale_0002_parallel_min_bytes_environment(int argc, char **argv)
{
    SIXELSTATUS status;
    char diagnostic[128];
    size_t value;
    size_t expected;

    status = SIXEL_OK;
    diagnostic[0] = '\0';
    expected = 17u;
    if (argc > 1 && strcmp(argv[1], "cli") == 0) {
        status = sixel_option_apply_runtime_policy_argument(
            "auto:B17",
            SIXEL_OPTION_SCOPE_ENCODER,
            diagnostic,
            sizeof(diagnostic));
        if (SIXEL_FAILED(status)) {
            return EXIT_FAILURE;
        }
    } else {
        if (argc > 1 && strcmp(argv[1], "negative") == 0) {
            expected = (size_t)-1;
            if (sixel_compat_setenv(
                    "SIXEL_SCALE_PARALLEL_MIN_BYTES", "-1") != 0) {
                return EXIT_FAILURE;
            }
        } else if (sixel_compat_setenv(
                       "SIXEL_SCALE_PARALLEL_MIN_BYTES", "17") != 0) {
            return EXIT_FAILURE;
        }
    }
    value = sixel_scale_parallel_min_bytes();
    (void)sixel_compat_setenv("SIXEL_SCALE_PARALLEL_MIN_BYTES", "");
    if (value != expected) {
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
