/* SPDX-License-Identifier: MIT
 * Object settings, side-specific defaults and nested scopes stay isolated.
 */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sixel.h>
#include "src/compat_stub.h"
#include "src/encoder.h"
#include "src/decoder.h"
#include "src/decoder-parallel.h"
#include "src/threading.h"

int
test_threadpool_0005_object_policy_isolation(int argc, char **argv)
{
    sixel_encoder_t *first;
    sixel_encoder_t *second;
    sixel_decoder_t *decoder;
    sixel_threading_snapshot_t a;
    sixel_threading_snapshot_t b;
    sixel_threading_snapshot_t d;
    sixel_thread_budget_scope_t outer;
    sixel_thread_budget_scope_t inner;
    int ok;

    (void)argc;
    (void)argv;
    first = NULL;
    second = NULL;
    decoder = NULL;
    ok = 0;
    if (SIXEL_FAILED(sixel_encoder_new(&first, NULL)) ||
        SIXEL_FAILED(sixel_encoder_new(&second, NULL)) ||
        SIXEL_FAILED(sixel_decoder_new(&decoder, NULL))) {
        goto end;
    }
    (void)sixel_compat_setenv("SIXEL_ENCODER_THREADING_POLICY", "greedy");
    (void)sixel_compat_setenv("SIXEL_DECODER_THREADING_POLICY", "greedy");
    (void)sixel_compat_setenv("SIXEL_ENCODER_THREADING_GLOBAL_BUDGET", "7");
    (void)sixel_compat_setenv("SIXEL_DECODER_THREADING_GLOBAL_BUDGET", "8");
    sixel_set_encoder_threads(3);
    sixel_set_decoder_threads(2);
    if (SIXEL_FAILED(sixel_encoder_setopt(first, '=', "lazy:B6:S2"))) {
        goto end;
    }
    sixel_threading_policy_resolve(&first->threading, 0, &a);
    sixel_threading_policy_resolve(&second->threading, 0, &b);
    sixel_threading_policy_resolve(&decoder->threading, 1, &d);
    ok = a.budget == sixel_threads_normalize(6)
        && a.options.mode == SIXEL_THREADING_POLICY_LAZY
        && b.budget == sixel_threads_normalize(3)
        && b.options.mode == SIXEL_THREADING_POLICY_GREEDY
        && d.budget == sixel_threads_normalize(2)
        && d.options.mode == SIXEL_THREADING_POLICY_GREEDY;
    ok &= sixel_decoder_setopt(decoder, '=', "greedy:S2")
        == SIXEL_BAD_ARGUMENT;
    /* An explicit nested object starts its own operation. Returning from
     * it restores the caller's policy, including its lazy parsing rule. */
    sixel_threading_policy_enter(&outer, &a);
    ok &= sixel_decoder_parallel_resolve_threads() == 1;
    sixel_threading_policy_enter(&inner, &d);
    ok &= sixel_decoder_parallel_resolve_threads() == d.budget;
    sixel_thread_budget_leave(&inner);
    ok &= sixel_threading_policy_mode() == SIXEL_THREADING_POLICY_LAZY;
    sixel_thread_budget_leave(&outer);
    /* The builtin loader obeys the encoder budget and decode stage cap. */
    (void)sixel_encoder_setopt(first, '=', "greedy:B6:S2");
    sixel_threading_object_enter(&outer, &first->threading, 0);
    ok &= sixel_decoder_parallel_resolve_threads()
        == sixel_threads_normalize(2);
    sixel_thread_budget_leave(&outer);
    /* A base-only update preserves this object's explicit budget. */
    (void)sixel_encoder_setopt(first, '=', "lazy");
    sixel_threading_policy_resolve(&first->threading, 0, &a);
    ok &= a.budget == sixel_threads_normalize(6);
    printf("isolation: explicit=%d independent=%d decoder=%d\n",
           a.budget, b.budget, d.budget);
end:
    sixel_encoder_unref(first);
    sixel_encoder_unref(second);
    sixel_decoder_unref(decoder);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
