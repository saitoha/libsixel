/* SPDX-License-Identifier: MIT */
/*
 * Policy: docs/encoder/palette-application/lookup-policy/ordered.md
 */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include <stdlib.h>
#include <string.h>
#include "src/threading.h"
#include "ordered_state_common.h"

typedef struct ordered_parallel_gate {
    sixel_mutex_t mutex;
    sixel_cond_t condition;
    int start;
} ordered_parallel_gate_t;

typedef struct ordered_parallel_worker {
    ordered_parallel_gate_t *gate;
    int seed;
    int ok;
} ordered_parallel_worker_t;

static int
ordered_parallel_run(void *context)
{
    ordered_parallel_worker_t *worker;
    ordered_state_fixture_t fixture;
    int pass;

    worker = context;
    sixel_mutex_lock(&worker->gate->mutex);
    while (!worker->gate->start) {
        sixel_cond_wait(&worker->gate->condition, &worker->gate->mutex);
    }
    sixel_mutex_unlock(&worker->gate->mutex);
    worker->ok = 1;
    for (pass = 0; pass < 8 && worker->ok; ++pass) {
        /* Each caller owns its buffers but shares the Metal cache. */
        ordered_state_init(&fixture, pass % 2 ? 33 : 129,
                           worker->seed + pass);
        worker->ok = ordered_state_check(&fixture);
    }
    return 0;
}

int
test_gpupal_0025_ordered_parallel_cache(int argc, char **argv)
{
    ordered_parallel_gate_t gate;
    ordered_parallel_worker_t workers[3];
    sixel_thread_t threads[3];
    int started;
    int i;
    int ok;

    (void)argc;
    (void)argv;
    memset(&gate, 0, sizeof(gate));
    memset(workers, 0, sizeof(workers));
    memset(threads, 0, sizeof(threads));
    if (sixel_mutex_init(&gate.mutex) != 0) {
        return EXIT_FAILURE;
    }
    if (sixel_cond_init(&gate.condition) != 0) {
        sixel_mutex_destroy(&gate.mutex);
        return EXIT_FAILURE;
    }
    started = 0;
    for (i = 0; i < 3; ++i) {
        workers[i].gate = &gate;
        workers[i].seed = i * 19;
        if (sixel_thread_create(&threads[i], ordered_parallel_run,
                                &workers[i]) != 0) {
            break;
        }
        ++started;
    }
    /* Release already-created workers even if a later creation failed. */
    sixel_mutex_lock(&gate.mutex);
    gate.start = 1;
    sixel_cond_broadcast(&gate.condition);
    sixel_mutex_unlock(&gate.mutex);
    ok = started == 3;
    for (i = 0; i < started; ++i) {
        sixel_thread_join(&threads[i]);
        ok &= workers[i].ok;
    }
    sixel_cond_destroy(&gate.condition);
    sixel_mutex_destroy(&gate.mutex);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
