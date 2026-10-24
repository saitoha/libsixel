/* SPDX-License-Identifier: MIT */
/* Donated limits obey nested scopes and never change the global preference. */
#include <stdlib.h>
#include <sixel.h>
#include "src/threading.h"

static int
reclaim_ready(void *context)
{
    return *(int *)context;
}

int
test_parallel_float_0009(int argc, char **argv)
{
    sixel_thread_budget_scope_t donor;
    sixel_thread_budget_scope_t nested;
    int ready;
    int ok;

    (void)argc;
    (void)argv;
    ready = 0;
    sixel_set_encoder_threads(8);
    sixel_thread_budget_enter(&donor, 4);
    sixel_thread_budget_set_reclaim(&donor, 8, reclaim_ready, &ready);
    ok = sixel_threads_resolve() == 4 && sixel_threads_ceiling() == 8;
    ok &= sixel_threads_reclaim() == 4;
    sixel_thread_budget_enter(&nested, 2);
    ready = 1;
    ok &= sixel_threads_ceiling() == 2 && sixel_threads_reclaim() == 2;
    sixel_thread_budget_leave(&nested);
    ok &= sixel_threads_resolve() == 8;
    sixel_thread_budget_leave(&donor);
    ok &= sixel_threads_resolve() == 8;
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
