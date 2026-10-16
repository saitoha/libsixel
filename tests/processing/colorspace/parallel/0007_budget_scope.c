/* SPDX-License-Identifier: MIT */
/* Caller-local limits nest and restore without changing another thread. */
#include <stdlib.h>
#include <sixel.h>
#include "src/threading.h"

static int
parallel_budget_child(void *context)
{
    sixel_thread_budget_scope_t outer;
    sixel_thread_budget_scope_t inner;
    int *ok;

    ok = context;
    *ok = sixel_threads_resolve() == 8;
    sixel_thread_budget_enter(&outer, 4);
    *ok &= sixel_threads_resolve() == 4;
    sixel_thread_budget_enter(&inner, 7);
    *ok &= sixel_threads_resolve() == 4;
    sixel_thread_budget_leave(&inner);
    *ok &= sixel_threads_resolve() == 4;
    sixel_thread_budget_leave(&outer);
    *ok &= sixel_threads_resolve() == 8;
    return SIXEL_OK;
}

int
test_parallel_float_0007(int argc, char **argv)
{
    sixel_thread_budget_scope_t outer;
    sixel_thread_budget_scope_t inner;
    sixel_thread_t child;
    int child_ok;
    int ok;

    (void)argc;
    (void)argv;
    child_ok = 0;
    sixel_set_threads(8);
    sixel_thread_budget_enter(&outer, 3);
    ok = sixel_threads_resolve() == 3;
    sixel_thread_budget_enter(&inner, 2);
    ok &= sixel_threads_resolve() == 2;
    sixel_thread_budget_leave(&inner);
    ok &= sixel_threads_resolve() == 3;
    if (sixel_thread_create(&child, parallel_budget_child, &child_ok)
            == SIXEL_OK) {
        sixel_thread_join(&child);
    }
    ok &= child_ok && sixel_threads_resolve() == 3;
    sixel_thread_budget_leave(&outer);
    ok &= sixel_threads_resolve() == 8;
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
