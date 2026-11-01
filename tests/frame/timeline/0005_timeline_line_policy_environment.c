/*
 * SPDX-License-Identifier: MIT
 *
 * Characterize the legacy SIXEL_LOG_LINES parsing contract.
 */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>

#include "src/compat_stub.h"
#include "src/lookup-policy-fhedt.h"

static int
timeline_line_policy_matches(char const *text,
                             int expected_enabled,
                             int expected_stride)
{
    int enabled;
    int stride;

    enabled = -1;
    stride = -1;
    if (sixel_compat_setenv("SIXEL_LOG_LINES", text) != 0) {
        return 0;
    }
    sixel_lookup_policy_fhedt_resolve_timeline_lines(&enabled, &stride);
    return enabled == expected_enabled && stride == expected_stride;
}

int
test_timeline_0005_timeline_line_policy_environment(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    if (!timeline_line_policy_matches("", 0, 1)) {
        fprintf(stderr, "empty timeline line policy changed\n");
        return EXIT_FAILURE;
    }
    if (!timeline_line_policy_matches("0", 1, 1)) {
        fprintf(stderr, "zero timeline line policy changed\n");
        return EXIT_FAILURE;
    }
    if (!timeline_line_policy_matches("3", 1, 3)) {
        fprintf(stderr, "positive timeline line policy changed\n");
        return EXIT_FAILURE;
    }
    if (!timeline_line_policy_matches("3tail", 1, 3)) {
        fprintf(stderr, "prefix timeline line policy changed\n");
        return EXIT_FAILURE;
    }
    if (!timeline_line_policy_matches("abc", 1, 1)) {
        fprintf(stderr, "invalid timeline line policy changed\n");
        return EXIT_FAILURE;
    }
    if (!timeline_line_policy_matches("-5", 1, 1)) {
        fprintf(stderr, "negative timeline line policy changed\n");
        return EXIT_FAILURE;
    }

    (void)sixel_compat_setenv("SIXEL_LOG_LINES", "");
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
