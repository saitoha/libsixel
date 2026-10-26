/* SPDX-License-Identifier: MIT */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
#include <string.h>
#include "palette_store_test_common.h"

typedef struct palette_loan_probe {
    sixel_palette_scheduler_t scheduler;
    int phase;
    int proceed;
    int worker;
} palette_loan_probe_t;

static int
palette_loan_worker(void *context)
{
    palette_loan_probe_t *probe;
    sixel_palette_scheduler_t *scheduler;

    probe = context;
    scheduler = &probe->scheduler;
    sixel_palette_scheduler_background(scheduler, probe->worker, 1);
    sixel_mutex_lock(&scheduler->mutex);
    probe->phase = 1;
    sixel_cond_broadcast(&scheduler->changed);
    while (!probe->proceed) {
        sixel_cond_wait(&scheduler->changed, &scheduler->mutex);
    }
    sixel_mutex_unlock(&scheduler->mutex);
    /* Finish the borrowed job; the next job needs a new admission. */
    sixel_palette_scheduler_background(scheduler, probe->worker, 0);
    sixel_palette_scheduler_background(scheduler, probe->worker, 1);
    sixel_mutex_lock(&scheduler->mutex);
    probe->phase = 2;
    sixel_cond_broadcast(&scheduler->changed);
    sixel_mutex_unlock(&scheduler->mutex);
    sixel_palette_scheduler_background(scheduler, probe->worker, 0);
    return 0;
}

/* The condition establishes order. The deadline only escapes a broken
 * implementation; no elapsed-time threshold determines a passing result. */
static int
palette_loan_wait(palette_loan_probe_t *probe, int phase)
{
    sixel_palette_scheduler_t *scheduler;
    int failed;

    scheduler = &probe->scheduler;
    failed = 0;
    sixel_mutex_lock(&scheduler->mutex);
    while (phase ? probe->phase < phase : scheduler->waiting_jobs == 0) {
        if (sixel_cond_timedwait(&scheduler->changed,
                &scheduler->mutex, 5000)) {
            failed = 1;
            break;
        }
    }
    sixel_mutex_unlock(&scheduler->mutex);
    return failed;
}

int
test_palette_scheduler_0075(int argc, char **argv)
{
    palette_loan_probe_t probe;
    sixel_threading_snapshot_t policy;
    sixel_palette_lease_t p = {0}, q = {0}, m = {0};
    sixel_thread_t thread;
    int initialized, started, failed, i, budget, variant;

    (void)argc;
    (void)argv;
    initialized = started = 0;
    failed = 1;
    memset(&policy, 0, sizeof(policy));
    policy.options.palette = SIXEL_THREADING_PALETTE_LATEST;
    for (variant = 0; variant < 4; ++variant) {
        budget = variant % 2 ? 8 : 4;
        memset(&probe, 0, sizeof(probe));
        probe.worker = variant < 2 ? 1 : 2;
        policy.budget = budget;
        PALETTE_STORE_CHECK(sixel_palette_scheduler_init(&probe.scheduler,
            &policy) == SIXEL_OK);
        initialized = 1;
        PALETTE_STORE_CHECK(sixel_palette_scheduler_acquire(&probe.scheduler,
            SIXEL_PALETTE_LANE_BUILD, 0, &p) == budget / 2);
        PALETTE_STORE_CHECK(sixel_palette_scheduler_acquire(&probe.scheduler,
            SIXEL_PALETTE_LANE_PREPARE, 0, &q) == budget);
        PALETTE_STORE_CHECK(sixel_palette_scheduler_acquire(&probe.scheduler,
            SIXEL_PALETTE_LANE_APPLY, 0, &m) == budget);
        /* N active foreground jobs leave no loan, even with no controller. */
        for (i = 0; i < budget; ++i) {
            sixel_palette_scheduler_foreground(&probe.scheduler, 1, 1);
        }
        PALETTE_STORE_CHECK(sixel_thread_create(&thread,
            palette_loan_worker, &probe) == SIXEL_OK);
        started = 1;
        PALETTE_STORE_CHECK(!palette_loan_wait(&probe, 0));
        /* One finished foreground job lends exactly one slot. */
        sixel_palette_scheduler_foreground(&probe.scheduler, 1, 0);
        PALETTE_STORE_CHECK(!palette_loan_wait(&probe, 1));
        /* Reclaim while the borrower is held at a barrier. This call must
         * return before the test opens that barrier: M cannot wait for P. */
        sixel_palette_scheduler_foreground(&probe.scheduler, 0, 1);
        sixel_mutex_lock(&probe.scheduler.mutex);
        probe.proceed = 1;
        sixel_cond_broadcast(&probe.scheduler.changed);
        sixel_mutex_unlock(&probe.scheduler.mutex);
        PALETTE_STORE_CHECK(!palette_loan_wait(&probe, 0));
        sixel_mutex_lock(&probe.scheduler.mutex);
        i = probe.phase == 1 && probe.scheduler.background_jobs == 0;
        sixel_mutex_unlock(&probe.scheduler.mutex);
        PALETTE_STORE_CHECK(i);
        /* The next job resumes only after the controller returns its claim. */
        sixel_palette_scheduler_foreground(&probe.scheduler, 0, 0);
        PALETTE_STORE_CHECK(!palette_loan_wait(&probe, 2));
        sixel_thread_join(&thread);
        started = 0;
        for (i = 1; i < budget; ++i) {
            sixel_palette_scheduler_foreground(&probe.scheduler, 1, 0);
        }
        PALETTE_STORE_CHECK(probe.scheduler.background_jobs == 0 &&
            probe.scheduler.foreground_jobs == 0);
        fprintf(stderr, "latest loan trace: N=%d M=%d reclaim=no-wait\n",
            budget, m.threads);
        sixel_palette_lease_release(&m);
        sixel_palette_lease_release(&q);
        sixel_palette_lease_release(&p);
        sixel_palette_scheduler_destroy(&probe.scheduler);
        initialized = 0;
    }
    failed = 0;
end:
    if (initialized) {
        sixel_mutex_lock(&probe.scheduler.mutex);
        probe.proceed = 1;
        probe.scheduler.foreground_controller = 0;
        probe.scheduler.foreground_jobs = 0;
        sixel_cond_broadcast(&probe.scheduler.changed);
        sixel_mutex_unlock(&probe.scheduler.mutex);
    }
    if (started) {
        sixel_thread_join(&thread);
    }
    sixel_palette_lease_release(&m);
    sixel_palette_lease_release(&q);
    sixel_palette_lease_release(&p);
    if (initialized) {
        sixel_palette_scheduler_destroy(&probe.scheduler);
    }
    return failed;
}
