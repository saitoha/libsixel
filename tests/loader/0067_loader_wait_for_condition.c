/*
 * SPDX-License-Identifier: MIT
 *
 * Verify that the loader's bounded wait helper recognizes readiness before
 * its deadline and reports an unmet condition at the deadline.
 */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>

#include "src/loader.h"

typedef struct wait_probe_state {
    int trigger_after;
    int calls;
} wait_probe_state_t;

static int
wait_probe_predicate(void *context)
{
    wait_probe_state_t *state;

    state = (wait_probe_state_t *)context;
    if (state == NULL || state->trigger_after < 0) {
        return 0;
    }
    state->calls += 1;
    return state->calls >= state->trigger_after;
}

int
test_loader_0067_loader_wait_for_condition(int argc, char **argv)
{
    wait_probe_state_t immediate;
    wait_probe_state_t delayed;
    wait_probe_state_t never;
    int ready;

    (void)argc;
    (void)argv;
    immediate.trigger_after = 0;
    immediate.calls = 0;
    delayed.trigger_after = 3;
    delayed.calls = 0;
    never.trigger_after = -1;
    never.calls = 0;
    ready = sixel_loader_wait_for_condition(wait_probe_predicate,
                                            &immediate,
                                            0);
    if (ready == 0) {
        fprintf(stderr, "wait helper should finish immediately\n");
        return EXIT_FAILURE;
    }
    ready = sixel_loader_wait_for_condition(wait_probe_predicate,
                                            &delayed,
                                            8);
    if (ready == 0) {
        fprintf(stderr, "wait helper should complete before timeout\n");
        return EXIT_FAILURE;
    }
    ready = sixel_loader_wait_for_condition(wait_probe_predicate,
                                            &never,
                                            2);
    if (ready != 0) {
        fprintf(stderr, "wait helper should report timeout\n");
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
