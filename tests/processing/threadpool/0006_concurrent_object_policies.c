/* SPDX-License-Identifier: MIT
 * Real encoder calls on two native threads retain separate planner results.
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
#include "src/threading.h"
#include "tests/planner/pipeline/playback_test_common.h"

#if SIXEL_ENABLE_THREADS
typedef struct policy_barrier {
    sixel_mutex_t mutex;
    sixel_cond_t cond;
    int waiting;
    int generation;
} policy_barrier_t;

typedef struct policy_worker {
    policy_barrier_t *barrier;
    sixel_encoder_t *encoder;
    int budget;
    int lazy;
    int ok;
} policy_worker_t;

static void
policy_barrier_wait(policy_barrier_t *barrier)
{
    int generation;

    sixel_mutex_lock(&barrier->mutex);
    generation = barrier->generation;
    if (++barrier->waiting == 2) {
        barrier->waiting = 0;
        ++barrier->generation;
        sixel_cond_broadcast(&barrier->cond);
    } else {
        while (generation == barrier->generation) {
            sixel_cond_wait(&barrier->cond, &barrier->mutex);
        }
    }
    sixel_mutex_unlock(&barrier->mutex);
}

static int
policy_discard(char *bytes, int size, void *context)
{
    (void)bytes;
    (void)context;
    return size;
}

static int
policy_encode_worker(void *context)
{
    policy_worker_t *worker;
    sixel_frame_t *frame;
    sixel_output_t *output;
    sixel_playback_t *playback;
    sixel_threading_snapshot_t outside;
    SIXELSTATUS status;
    int iteration;

    worker = (policy_worker_t *)context;
    output = NULL;
    playback = NULL;
    worker->ok = sixel_output_new(&output, policy_discard, NULL, NULL)
        == SIXEL_OK;
    for (iteration = 0; iteration < 8; ++iteration) {
        policy_barrier_wait(worker->barrier);
        frame = playback_test_frame(iteration);
        status = SIXEL_BAD_ALLOCATION;
        if (frame != NULL && output != NULL) {
            if (iteration < 4) {
                status = sixel_encoder_encode_frame(worker->encoder,
                                                     frame, output);
            } else {
                status = sixel_playback_new(&playback, worker->encoder,
                                             output, NULL, NULL);
                if (status == SIXEL_OK) {
                    status = sixel_playback_submit(playback, frame,
                        SIXEL_PLAYBACK_WAIT, -1, NULL);
                    if (status == SIXEL_OK) {
                        status = sixel_playback_drain(playback, -1);
                    }
                    sixel_playback_unref(playback);
                    playback = NULL;
                }
            }
        }
        sixel_frame_unref(frame);
        worker->ok &= status == SIXEL_OK;
        worker->ok &= worker->encoder->planner.total_threads == worker->budget;
        if (worker->lazy) {
            worker->ok &= worker->encoder->planner.pipeline_dither_threads == 1;
        } else {
            worker->ok &= worker->encoder->planner.pipeline_dither_threads > 1;
        }
        /* Public calls and their helper threads must remove every scope. */
        worker->ok &= !sixel_threading_policy_capture(&outside, 0);
    }
    sixel_output_unref(output);
    return 0;
}
#endif

int
test_threadpool_0006_concurrent_object_policies(int argc, char **argv)
{
#if SIXEL_ENABLE_THREADS
    policy_barrier_t barrier;
    policy_worker_t workers[2];
    sixel_thread_t threads[2];
    int index;
    int ok;

    (void)argc;
    (void)argv;
    memset(&barrier, 0, sizeof(barrier));
    memset(workers, 0, sizeof(workers));
    memset(threads, 0, sizeof(threads));
    (void)sixel_compat_setenv("SIXEL_ENCODER_THREADING_POLICY", "greedy");
    sixel_set_encoder_threads(3);
    if (sixel_mutex_init(&barrier.mutex) != SIXEL_OK) {
        return EXIT_FAILURE;
    }
    if (sixel_cond_init(&barrier.cond) != SIXEL_OK) {
        sixel_mutex_destroy(&barrier.mutex);
        return EXIT_FAILURE;
    }
    ok = 1;
    for (index = 0; index < 2; ++index) {
        workers[index].barrier = &barrier;
        workers[index].budget = index == 0 ? 3 : 6;
        workers[index].lazy = index;
        ok &= sixel_encoder_new(&workers[index].encoder, NULL) == SIXEL_OK;
        if (workers[index].encoder != NULL) {
            ok &= sixel_encoder_setopt(workers[index].encoder,
                                        'd', "fs") == SIXEL_OK;
        }
    }
    if (ok) {
        ok &= sixel_encoder_setopt(workers[1].encoder,
                                    '=', "lazy:B6") == SIXEL_OK;
    }
    if (ok) {
        /* A creation failure cannot leave the first worker at a barrier. */
        if (sixel_thread_create(&threads[0], policy_encode_worker,
                                &workers[0]) != SIXEL_OK) {
            ok = 0;
        } else if (sixel_thread_create(&threads[1], policy_encode_worker,
                                       &workers[1]) != SIXEL_OK) {
            (void)policy_encode_worker(&workers[1]);
            ok = 0;
        }
        sixel_thread_join(&threads[0]);
        sixel_thread_join(&threads[1]);
        ok &= workers[0].ok && workers[1].ok;
    }
    if (workers[0].encoder != NULL && workers[1].encoder != NULL) {
        printf("# concurrent: API greedy budget=%d dither=%d; "
           "explicit lazy budget=%d dither=%d\n",
           workers[0].encoder->planner.total_threads,
           workers[0].encoder->planner.pipeline_dither_threads,
           workers[1].encoder->planner.total_threads,
           workers[1].encoder->planner.pipeline_dither_threads);
    }
    for (index = 0; index < 2; ++index) {
        sixel_encoder_unref(workers[index].encoder);
    }
    sixel_cond_destroy(&barrier.cond);
    sixel_mutex_destroy(&barrier.mutex);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
#else
    (void)argc;
    (void)argv;
    return EXIT_SUCCESS;
#endif
}
