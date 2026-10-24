/*
 * SPDX-License-Identifier: MIT
 * Exercise policy precedence, topology caps, scopes and mode-specific work.
 */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sixel.h>

#include "src/compat_stub.h"
#include "src/decoder-parallel.h"
#include "src/options.h"
#include "src/planner.h"
#include "src/threading.h"

static int
policy_expect(char const *name, int actual, int expected)
{
    if (actual == expected) {
        return 1;
    }
    fprintf(stderr, "%s: got %d, expected %d\n", name, actual, expected);
    return 0;
}

static sixel_threading_policy_options_t policy_options;
static sixel_thread_budget_scope_t policy_scope;
static int policy_active;

static void
policy_reset(void)
{
    if (policy_active) {
        sixel_thread_budget_leave(&policy_scope);
        policy_active = 0;
    }
    memset(&policy_options, 0, sizeof(policy_options));
}

static int
policy_apply(char const *text, SIXELSTATUS expected)
{
    SIXELSTATUS status;
    if (policy_active) {
        sixel_thread_budget_leave(&policy_scope);
    }
    status = sixel_option_apply_threading_policy_argument(
        &policy_options, text, SIXEL_OPTION_SCOPE_ENCODER, NULL, 0u);
    sixel_threading_object_enter(&policy_scope, &policy_options, 0);
    policy_active = 1;
    return policy_expect(text, status, expected);
}

int
test_threadpool_0003_thread_policy(int argc, char **argv)
{
    sixel_runtime_policy_options_t runtime;
    sixel_thread_budget_scope_t scope;
    sixel_dither_encode_request_t request;
    sixel_dither_encode_allocation_t allocation;
    int ok;
    int hardware;
    int performance;
    int expected;
    int budget;
    size_t index;
    char const *modes[] = { "lazy", "balanced", "greedy" };
    char const *invalid_env[] = { "0", "-1", "none" };
    char const *invalid[] = {
        "auto", "serial", "4", "0", "balanced:B0", "balanced:B-1",
        "balanced:global_budget=2147483648", "balanced:Bnone",
        "balanced:global_budget=0", "balanced:global_budget=none"
    };

    (void)argc;
    (void)argv;
    memset(&runtime, 0, sizeof(runtime));
    memset(&request, 0, sizeof(request));
    policy_reset();
    ok = sixel_compat_setenv("SIXEL_ENCODER_THREADING_POLICY", "") == 0;
    ok &= sixel_compat_setenv("SIXEL_ENCODER_THREADING_GLOBAL_BUDGET",
                                  "") == 0;
    ok &= sixel_compat_setenv("SIXEL_ENCODER_THREADING_DECODE", "") == 0;
    hardware = sixel_threads_normalize(0);
    performance = sixel_get_performance_threads();
    expected = performance < 4 ? performance : 4;
#if !SIXEL_ENABLE_THREADS
    expected = 1;
#endif
    ok &= policy_expect("library processing", sixel_threads_resolve(),
                         expected);
    ok &= policy_expect("library parsing",
                         sixel_decoder_parallel_resolve_threads(), 1);
    ok &= policy_expect("no explicit budget",
                         sixel_threading_policy_explicit_budget(NULL), 0);

    for (index = 0u; index < sizeof(invalid_env) / sizeof(invalid_env[0]);
            ++index) {
        ok &= sixel_compat_setenv("SIXEL_ENCODER_THREADING_GLOBAL_BUDGET",
                                  invalid_env[index]) == 0;
        ok &= policy_expect("invalid environment keeps default",
                             sixel_threads_resolve(), expected);
        ok &= policy_expect("invalid environment keeps serial parsing",
                             sixel_decoder_parallel_resolve_threads(), 1);
    }
    ok &= sixel_compat_setenv("SIXEL_ENCODER_THREADING_GLOBAL_BUDGET",
                                  "") == 0;
    ok &= sixel_compat_setenv("SIXEL_ENCODER_THREADING_DECODE", "99") == 0;
    ok &= policy_expect("decode limit cannot enable parsing",
                         sixel_decoder_parallel_resolve_threads(), 1);
    ok &= sixel_compat_setenv("SIXEL_ENCODER_THREADING_DECODE", "") == 0;
    for (index = 0u; index < sizeof(modes) / sizeof(modes[0]); ++index) {
        ok &= policy_apply(modes[index], SIXEL_OK);
        ok &= policy_expect("mode-only library budget",
                             sixel_threads_resolve(), expected);
    }
    policy_reset();
    ok &= sixel_compat_setenv("SIXEL_ENCODER_THREADING_POLICY", "greedy") == 0;
    ok &= policy_expect("explicit mode default", sixel_threads_resolve(),
                         expected);
    ok &= policy_expect("encoder environment leaves parsing serial",
                         sixel_decoder_parallel_resolve_threads(), 1);
    ok &= sixel_compat_setenv("SIXEL_ENCODER_THREADING_GLOBAL_BUDGET",
                                  "3") == 0;
    ok &= policy_expect("environment budget", sixel_threads_resolve(),
                         sixel_threads_normalize(3));
    ok &= sixel_compat_setenv("SIXEL_ENCODER_THREADING_GLOBAL_BUDGET",
                                  "aLl") == 0;
    ok &= policy_expect("environment all", sixel_threads_resolve(), hardware);
    ok &= policy_expect("encoder all leaves parsing serial",
                         sixel_decoder_parallel_resolve_threads(), 1);
    ok &= sixel_compat_setenv("SIXEL_ENCODER_THREADING_GLOBAL_BUDGET",
                                  "3") == 0;
    sixel_set_encoder_threads(0);
    ok &= policy_expect("API zero means all", sixel_threads_resolve(),
                         hardware);
    sixel_set_encoder_threads(-1);
    ok &= policy_expect("API negative means all", sixel_threads_resolve(),
                         hardware);
    sixel_set_encoder_threads(4);
    ok &= policy_expect("API beats environment", sixel_threads_resolve(),
                         sixel_threads_normalize(4));
    ok &= policy_apply("lazy", SIXEL_OK);
    ok &= policy_expect("base preserves API budget", sixel_threads_resolve(),
                         sixel_threads_normalize(4));
    ok &= policy_expect("lazy fanout", sixel_threads_resolve_fanout(), 1);
    ok &= policy_expect("lazy parser",
                         sixel_decoder_parallel_resolve_threads(), 1);
    ok &= policy_expect("lazy load", sixel_threading_policy_load_mode(),
                         SIXEL_THREADING_FRAME_AHEAD);
    ok &= policy_expect("lazy prepare", sixel_threading_policy_prepare_mode(),
                         SIXEL_THREADING_FRAME_AHEAD);
    ok &= policy_apply("balanced:B2147483647", SIXEL_OK);
    expected = performance;
#if !SIXEL_ENABLE_THREADS
    expected = 1;
#endif
    ok &= policy_expect("balanced cap", sixel_threads_resolve(), expected);
    ok &= policy_expect("balanced parser cap",
                         sixel_decoder_parallel_resolve_threads(), expected);
    ok &= policy_apply("balanced:Ball", SIXEL_OK);
    ok &= policy_expect("balanced all cap", sixel_threads_resolve(), expected);
    ok &= policy_apply("greedy:global_budget=ALL", SIXEL_OK);
    ok &= policy_expect("CLI all", sixel_threads_resolve(), hardware);
    ok &= policy_apply("greedy:B3", SIXEL_OK);
    sixel_set_encoder_threads(1);
    ok &= policy_expect("CLI beats later API", sixel_threads_resolve(),
                         sixel_threads_normalize(3));
    ok &= policy_apply("balanced", SIXEL_OK);
    ok &= policy_expect("base preserves CLI budget", sixel_threads_resolve(),
                         performance < 3 ? performance : 3);
    ok &= policy_apply("greedy:B8:decode=99", SIXEL_OK);
    budget = sixel_threads_normalize(8);
    ok &= policy_expect("greedy budget", sixel_threads_resolve(), budget);
    ok &= policy_expect("decode shrinks to budget",
                         sixel_decoder_parallel_resolve_threads(), budget);
    sixel_thread_budget_enter(&scope, 1);
    ok &= policy_expect("nested scope", sixel_threads_resolve(), 1);
    sixel_thread_budget_leave(&scope);
    ok &= policy_expect("scope restored", sixel_threads_resolve(), budget);

    /* Explicit runtime thresholds lose to greedy, even across mode changes. */
    runtime.colorspace_parallel_min_pixels = 123u;
    runtime.colorspace_parallel_min_pixels_override = 1;
    runtime.sample_parallel_min_pixels = 456u;
    runtime.sample_parallel_min_pixels_override = 1;
    runtime.scale_parallel_min_bytes = 789u;
    runtime.scale_parallel_min_bytes_override = 1;
    sixel_runtime_policy_store(&runtime);
    ok &= policy_expect("greedy colorspace",
        (int)sixel_runtime_policy_colorspace_min_pixels(999u), 0);
    ok &= policy_expect("greedy sample",
        (int)sixel_runtime_policy_sample_min_pixels(999u), 0);
    ok &= policy_expect("greedy scale",
        (int)sixel_runtime_policy_scale_min_bytes(999u), 0);
    ok &= policy_apply("balanced:B1", SIXEL_OK);
    ok &= policy_expect("balanced colorspace",
        (int)sixel_runtime_policy_colorspace_min_pixels(999u), 123);
    ok &= policy_expect("balanced sample",
        (int)sixel_runtime_policy_sample_min_pixels(999u), 456);
    ok &= policy_expect("balanced scale",
        (int)sixel_runtime_policy_scale_min_bytes(999u), 789);
    for (index = 0; index < sizeof(invalid) / sizeof(invalid[0]); ++index) {
        ok &= policy_apply(invalid[index], SIXEL_BAD_ARGUMENT);
        if (strchr(invalid[index], ':') != NULL) {
            ok &= policy_expect("budget diagnostic names all",
                strstr(sixel_helper_get_additional_message(),
                       "positive integer or all") != NULL, 1);
        }
        ok &= policy_expect("failed parse is atomic",
                             sixel_threads_resolve(), 1);
    }
    ok &= policy_apply("lazy:palette=latest", SIXEL_NOT_IMPLEMENTED);
    ok &= policy_expect("unimplemented parse is atomic",
                         sixel_threading_policy_mode(),
                         SIXEL_THREADING_POLICY_BALANCED);

    /* Lazy must not stage or split dither unless the dither key is set. */
    request.total_threads = 8;
    request.height = 96;
    request.ncolors = 256;
    request.method_for_diffuse = SIXEL_DIFFUSE_FS;
    request.lazy = 1;
    request.prefer_staged = 1;
    sixel_dither_encode_allocate(&request, &allocation);
    ok &= policy_expect("lazy allocation", allocation.mode,
                         SIXEL_DITHER_ENCODE_PIPELINE);
    ok &= policy_expect("lazy dither", allocation.dither_threads, 1);
    ok &= policy_expect("lazy encode", allocation.encode_threads, 7);
    request.threads_max = 4u;
    request.threads_max_set = 1;
    sixel_dither_encode_allocate(&request, &allocation);
    ok &= policy_expect("explicit lazy dither", allocation.dither_threads, 4);
    ok &= policy_expect("explicit lazy encode", allocation.encode_threads, 4);
    policy_reset();
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
