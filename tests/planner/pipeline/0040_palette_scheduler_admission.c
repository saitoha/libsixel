/* SPDX-License-Identifier: MIT */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
#include <string.h>
#include "palette_store_test_common.h"

int
test_palette_scheduler_0040(int argc, char **argv)
{
    sixel_palette_scheduler_t scheduler;
    sixel_threading_snapshot_t policy;
    sixel_palette_lease_t p = {0}, q = {0}, m = {0}, next = {0};
    sixel_dither_encode_request_t request;
    sixel_dither_encode_allocation_t allocation;
    int budgets[] = {2, 3, 4, 16};
    int used[3];
    int mode, n, w, caps, expected, failed, initialized;

    (void)argc;
    (void)argv;
    failed = 1;
    initialized = 0;
    memset(&policy, 0, sizeof(policy));
    memset(&request, 0, sizeof(request));
    policy.options.palette = SIXEL_THREADING_PALETTE_LATEST;
    policy.options.prepare_threads_override = 1;
    request.height = 600;
    request.ncolors = 256;
    request.method_for_diffuse = SIXEL_DIFFUSE_FS;
    request.prefer_staged = 1;
    for (mode = 0; mode < 3; ++mode) {
        policy.options.mode = mode;
        for (n = 0; n < 4; ++n) {
            policy.budget = budgets[n];
            for (w = 1; w <= policy.budget; ++w) {
                policy.options.prepare_threads = (unsigned int)w;
                for (caps = 0; caps < 3; ++caps) {
                    policy.options.dither_threads_override = caps != 0;
                    policy.options.encode_threads_override = caps != 0;
                    policy.options.dither_threads = caps == 1 ? 1 : 100;
                    policy.options.encode_threads = caps == 1 ? 1 : 100;
                    PALETTE_STORE_CHECK(sixel_palette_scheduler_init(
                        &scheduler, &policy) == SIXEL_OK);
                    initialized = 1;
                    expected = mode == SIXEL_THREADING_POLICY_LAZY
                        ? 1 : policy.budget / 2;
                    if (expected > w) {
                        expected = w;
                    }
                    PALETTE_STORE_CHECK(sixel_palette_scheduler_acquire(
                        &scheduler, SIXEL_PALETTE_LANE_BUILD, 0, &p)
                        == expected);
                    (void)sixel_palette_scheduler_acquire(&scheduler,
                        SIXEL_PALETTE_LANE_PREPARE, 0, &q);
                    PALETTE_STORE_CHECK(sixel_palette_scheduler_acquire(
                        &scheduler, SIXEL_PALETTE_LANE_APPLY, 0, &m) >= 1);
                    sixel_palette_scheduler_usage(&scheduler, used);
                    PALETTE_STORE_CHECK(used[0] <= w && used[1] <= w &&
                        used[2] == policy.budget && m.threads == policy.budget);
                    PALETTE_STORE_CHECK(w != 1 ||
                        (p.threads == 1 && q.threads == 1));
                    sixel_palette_lease_allocate(&m, &request, &allocation);
                    PALETTE_STORE_CHECK(allocation.dither_threads <=
                        m.threads && allocation.encode_threads <= m.threads);
                    PALETTE_STORE_CHECK(allocation.mode !=
                        SIXEL_DITHER_ENCODE_PIPELINE ||
                        allocation.dither_threads + allocation.encode_threads
                            <= m.threads);
                    /* Transfer next preparation only after its scope ends;
                     * a completed palette still owns P until explicit join. */
                    sixel_palette_lease_release(&q);
                    (void)sixel_palette_scheduler_acquire(&scheduler,
                        SIXEL_PALETTE_LANE_BUILD, 0, &next);
                    sixel_palette_scheduler_usage(&scheduler, used);
                    PALETTE_STORE_CHECK(used[0] <= w && used[1] <= w &&
                        used[2] == policy.budget && m.threads == policy.budget);
                    sixel_palette_lease_release(&next);
                    sixel_palette_lease_release(&p);
                    sixel_palette_lease_release(&m);
                    sixel_palette_scheduler_usage(&scheduler, used);
                    PALETTE_STORE_CHECK(used[0] + used[1] + used[2] == 0);
                    sixel_palette_scheduler_destroy(&scheduler);
                    initialized = 0;
                }
            }
        }
    }
    failed = 0;
end:
    sixel_palette_lease_release(&next);
    sixel_palette_lease_release(&p);
    sixel_palette_lease_release(&q);
    sixel_palette_lease_release(&m);
    if (initialized) {
        sixel_palette_scheduler_destroy(&scheduler);
    }
    return failed;
}
