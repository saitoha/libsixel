/* SPDX-License-Identifier: MIT */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
#include <string.h>
#include "../../_support/planner/pipeline/palette_launch_test_common.h"

static void
palette_sync_observe_budget(void *context)
{
    int *threads;

    threads = context;
    *threads = sixel_threads_resolve();
}

int
test_palette_scheduler_0044(int argc, char **argv)
{
    palette_store_fixture_t f;
    sixel_encoder_t *encoder = NULL;
    sixel_frame_t *source = NULL, *sample = NULL, *reference = NULL;
    sixel_palette_build_request_t *request = NULL;
    sixel_palette_store_job_t *job;
    sixel_palette_job_test_hooks_t hooks = {0};
    sixel_palette_selection_t result = {0};
    sixel_palette_frame_state_t state;
    sixel_dither_t *expected = NULL, *actual = NULL;
    sixel_palette_sampling_policy_t sampling;
    sixel_thread_budget_scope_t budget;
    int observed;
    int used[3];
    int floating, policy, failed;

    (void)argc;
    (void)argv;
    failed = 1;
    hooks.fail_thread_create = 1;
    hooks.before_publish = palette_sync_observe_budget;
    hooks.context = &observed;
    PALETTE_STORE_CHECK(!palette_store_fixture_init(&f));
    for (floating = 0; floating < 2; ++floating) {
        for (policy = 1; policy <= 2; ++policy) {
            sampling = (sixel_palette_sampling_policy_t)policy;
            PALETTE_STORE_CHECK(!palette_launch_input(f.allocator, floating,
                sampling, &encoder, &source, &request, &sample));
            PALETTE_STORE_CHECK(palette_launch_clone(sample, f.allocator,
                &reference) == SIXEL_OK);
            PALETTE_STORE_CHECK(sixel_palette_build_request_execute(request,
                reference, &expected, &state, NULL, NULL) == SIXEL_OK);
            PALETTE_STORE_CHECK(sixel_palette_store_admit(f.store, 1,
                &request, &sample, &hooks, &job) == SIXEL_OK);
            PALETTE_STORE_CHECK(sixel_palette_store_job_start(job,
                &f.scheduler) == SIXEL_RUNTIME_ERROR);
            sixel_palette_scheduler_usage(&f.scheduler, used);
            PALETTE_STORE_CHECK(used[0] + used[1] + used[2] == 0);
            /* Re-sampling this source on failure would now build black. */
            memset(sixel_frame_get_pixels(source), 0,
                (size_t)768 * (floating ? sizeof(float) : 1));
            observed = 0;
            sixel_thread_budget_enter(&budget, 1);
            sixel_palette_store_job_run(job);
            sixel_thread_budget_leave(&budget);
            PALETTE_STORE_CHECK(observed == 1);
            PALETTE_STORE_CHECK(sixel_palette_store_job_join(job, SIXEL_OK)
                == SIXEL_OK);
            sixel_palette_store_job_result(job, &result);
            PALETTE_STORE_CHECK(sixel_palette_snapshot_clone(result.snapshot,
                &actual) == SIXEL_OK);
            PALETTE_STORE_CHECK(palette_launch_equal(actual, expected,
                floating));
            PALETTE_STORE_CHECK(sixel_palette_snapshot_profile(
                result.snapshot)->construction.sampling.effective == policy);
            sixel_palette_store_job_release(job);
            sixel_palette_store_reset(f.store, 0);
            sixel_palette_snapshot_unref(result.snapshot);
            result.snapshot = NULL;
            sixel_dither_unref(actual);
            actual = NULL;
            sixel_dither_unref(expected);
            expected = NULL;
            sixel_frame_unref(reference);
            reference = NULL;
            sixel_frame_unref(source);
            source = NULL;
            sixel_encoder_unref(encoder);
            encoder = NULL;
        }
    }
    failed = 0;
end:
    sixel_palette_snapshot_unref(result.snapshot);
    sixel_dither_unref(actual);
    sixel_dither_unref(expected);
    sixel_frame_unref(reference);
    sixel_frame_unref(source);
    sixel_frame_unref(sample);
    sixel_palette_build_request_free(request);
    sixel_encoder_unref(encoder);
    return palette_store_fixture_free(&f) || failed;
}
