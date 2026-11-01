/* SPDX-License-Identifier: MIT */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
#include <stdlib.h>
#include <string.h>
#include <sixel.h>
#include "src/encoder.h"

#if SIXEL_ENABLE_THREADS
typedef struct option_start {
    sixel_mutex_t mutex;
    sixel_cond_t cond;
    sixel_encoder_t *encoder;
    int ready;
    int release;
} option_start_t;

typedef struct option_call {
    option_start_t *start;
    int flag;
} option_call_t;

static int
conflicting_option(void *data)
{
    option_call_t *call;
    option_start_t *start;

    call = data;
    start = call->start;
    sixel_mutex_lock(&start->mutex);
    ++start->ready;
    sixel_cond_broadcast(&start->cond);
    while (!start->release) {
        sixel_cond_wait(&start->cond, &start->mutex);
    }
    sixel_mutex_unlock(&start->mutex);
    return sixel_encoder_setopt(start->encoder, call->flag, NULL);
}
#endif

int
test_progressive_0010(int argc, char **argv)
{
#if SIXEL_ENABLE_THREADS
    option_start_t start;
    option_call_t calls[2];
    sixel_thread_t workers[2];
    uint64_t revision;
    int launched;
    int i;
    int ok;
#endif

    (void)argc;
    (void)argv;
#if !SIXEL_ENABLE_THREADS
    return 77;
#else
    memset(&start, 0, sizeof(start));
    if (sixel_encoder_new(&start.encoder, NULL) != SIXEL_OK) {
        return EXIT_FAILURE;
    }
    if (sixel_mutex_init(&start.mutex) != SIXEL_OK) {
        sixel_encoder_unref(start.encoder);
        return EXIT_FAILURE;
    }
    if (sixel_cond_init(&start.cond) != SIXEL_OK) {
        sixel_mutex_destroy(&start.mutex);
        sixel_encoder_unref(start.encoder);
        return EXIT_FAILURE;
    }
    revision = start.encoder->configuration_revision;
    launched = 0;
    for (i = 0; i < 2; ++i) {
        calls[i].start = &start;
        calls[i].flag = i == 0 ? SIXEL_OPTFLAG_8BIT_MODE
            : SIXEL_OPTFLAG_PENETRATE;
        if (sixel_thread_create(&workers[i], conflicting_option,
                                &calls[i]) != 0) {
            break;
        }
        ++launched;
    }
    sixel_mutex_lock(&start.mutex);
    while (start.ready < launched) {
        sixel_cond_wait(&start.cond, &start.mutex);
    }
    start.release = 1;
    sixel_cond_broadcast(&start.cond);
    sixel_mutex_unlock(&start.mutex);
    for (i = 0; i < launched; ++i) {
        sixel_thread_join(&workers[i]);
    }
    ok = launched == 2;
    if (ok) {
        /* Either ordering is legal. Exactly one transaction commits, and
         * a failed transaction cannot leave the conflicting flag enabled. */
        ok = (workers[0].result == SIXEL_OK &&
              workers[1].result == SIXEL_BAD_ARGUMENT) ||
             (workers[1].result == SIXEL_OK &&
              workers[0].result == SIXEL_BAD_ARGUMENT);
        ok = ok && start.encoder->f8bit +
            start.encoder->penetrate_multiplexer == 1 &&
            start.encoder->configuration_revision == revision + 1;
    }
    sixel_cond_destroy(&start.cond);
    sixel_mutex_destroy(&start.mutex);
    sixel_encoder_unref(start.encoder);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
#endif
}
