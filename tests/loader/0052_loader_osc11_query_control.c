/*
 * Verify loader-side OSC11 query control helpers.
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
    if (state == NULL) {
        return 0;
    }

    if (state->trigger_after < 0) {
        return 0;
    }

    state->calls += 1;
    if (state->calls >= state->trigger_after) {
        return 1;
    }

    return 0;
}

static int
run_enable_flag_cases(void)
{
    if (!sixel_loader_is_osc11_bg_query_enabled("1")) {
        fprintf(stderr, "expected only \"1\" to enable OSC11 query\n");
        return 1;
    }
    if (sixel_loader_is_osc11_bg_query_enabled("01") ||
            sixel_loader_is_osc11_bg_query_enabled("true") ||
            sixel_loader_is_osc11_bg_query_enabled("") ||
            sixel_loader_is_osc11_bg_query_enabled(NULL)) {
        fprintf(stderr, "unexpected OSC11 query enable value accepted\n");
        return 1;
    }

    return 0;
}

static int
run_timeout_parse_cases(void)
{
    if (sixel_loader_parse_osc11_bg_query_timeout_ms(NULL) != 50 ||
            sixel_loader_parse_osc11_bg_query_timeout_ms("") != 50 ||
            sixel_loader_parse_osc11_bg_query_timeout_ms("-1") != 50 ||
            sixel_loader_parse_osc11_bg_query_timeout_ms("abc") != 50 ||
            sixel_loader_parse_osc11_bg_query_timeout_ms(
                "999999999999999999") != 50) {
        fprintf(stderr, "timeout parser did not fall back to default\n");
        return 1;
    }
    if (sixel_loader_parse_osc11_bg_query_timeout_ms("0") != 0 ||
            sixel_loader_parse_osc11_bg_query_timeout_ms("25") != 25) {
        fprintf(stderr, "timeout parser did not accept valid values\n");
        return 1;
    }

    return 0;
}

static int
run_wait_cases(void)
{
    wait_probe_state_t immediate;
    wait_probe_state_t delayed;
    wait_probe_state_t never;
    int ready;

    immediate.trigger_after = 0;
    immediate.calls = 0;
    delayed.trigger_after = 3;
    delayed.calls = 0;
    never.trigger_after = -1;
    never.calls = 0;
    ready = 0;

    ready = sixel_loader_wait_for_condition(wait_probe_predicate,
                                            &immediate,
                                            0);
    if (ready == 0) {
        fprintf(stderr, "wait helper should finish immediately\n");
        return 1;
    }

    ready = sixel_loader_wait_for_condition(wait_probe_predicate,
                                            &delayed,
                                            8);
    if (ready == 0) {
        fprintf(stderr, "wait helper should complete before timeout\n");
        return 1;
    }

    ready = sixel_loader_wait_for_condition(wait_probe_predicate,
                                            &never,
                                            2);
    if (ready != 0) {
        fprintf(stderr, "wait helper should report timeout\n");
        return 1;
    }

    return 0;
}

int
test_loader_0052_loader_osc11_query_control(int argc, char **argv)
{
    int status;

    (void)argc;
    (void)argv;

    status = run_enable_flag_cases();
    if (status != 0) {
        return EXIT_FAILURE;
    }

    status = run_timeout_parse_cases();
    if (status != 0) {
        return EXIT_FAILURE;
    }

    status = run_wait_cases();
    if (status != 0) {
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
