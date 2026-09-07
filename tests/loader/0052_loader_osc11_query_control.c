/*
 * Verify loader-side OSC11 query eligibility.
 * Policy: docs/loader/alpha-policy.md
 * Policy: docs/loader/background-policy.md
 */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>

#include "src/loader.h"

static int
run_query_gate_cases(void)
{
    if (!sixel_loader_should_query_osc11_bgcolor(
            1, 0, 1, 0, SIXEL_ALPHA_POLICY_COMPOSITE) ||
            !sixel_loader_should_query_osc11_bgcolor(
                1, 0, 0, 1, SIXEL_ALPHA_POLICY_COMPOSITE)) {
        fprintf(stderr, "enabled query should use either tty stream\n");
        return 1;
    }
    if (sixel_loader_should_query_osc11_bgcolor(
            0, 0, 1, 1, SIXEL_ALPHA_POLICY_COMPOSITE) ||
            sixel_loader_should_query_osc11_bgcolor(
                1, 1, 1, 1, SIXEL_ALPHA_POLICY_COMPOSITE) ||
            sixel_loader_should_query_osc11_bgcolor(
                1, 0, 0, 0, SIXEL_ALPHA_POLICY_COMPOSITE) ||
            sixel_loader_should_query_osc11_bgcolor(
                1, 0, 1, 1, SIXEL_ALPHA_POLICY_CLEAR) ||
            sixel_loader_should_query_osc11_bgcolor(
                1, 0, 1, 1, SIXEL_ALPHA_POLICY_KEEP)) {
        fprintf(stderr, "disabled OSC11 query gate was accepted\n");
        return 1;
    }

    return 0;
}

int
test_loader_0052_loader_osc11_query_control(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return run_query_gate_cases() == 0
        ? EXIT_SUCCESS
        : EXIT_FAILURE;
}

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
