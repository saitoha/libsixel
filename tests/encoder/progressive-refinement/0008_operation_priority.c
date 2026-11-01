/* SPDX-License-Identifier: MIT */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
#include <stdlib.h>
#include <string.h>
#include <sixel.h>
#include "src/encoder.h"

#if SIXEL_ENABLE_THREADS
typedef struct operation_probe {
    sixel_encoder_t *encoder;
    SIXELSTATUS repair;
} operation_probe_t;

static int
operation_waiter(void *data)
{
    operation_probe_t *probe;

    probe = data;
    probe->repair = sixel_encoder_operation_enter(probe->encoder, 1);
    return sixel_encoder_set_6delta_threshold_limit(probe->encoder, 17);
}
#endif

int
test_progressive_0008(int argc, char **argv)
{
#if SIXEL_ENABLE_THREADS
    sixel_encoder_t *encoder;
    sixel_thread_t worker;
    operation_probe_t probe;
    int ok;
#endif

    (void)argc;
    (void)argv;
#if !SIXEL_ENABLE_THREADS
    return 77;
#else
    encoder = NULL;
    if (sixel_encoder_new(&encoder, NULL) != SIXEL_OK) {
        return EXIT_FAILURE;
    }
    probe.encoder = encoder;
    probe.repair = SIXEL_OK;
    if (sixel_encoder_operation_enter(encoder, 1) != SIXEL_OK) {
        sixel_encoder_unref(encoder);
        return EXIT_FAILURE;
    }
    if (sixel_thread_create(&worker, operation_waiter, &probe) != 0) {
        sixel_encoder_operation_leave(encoder);
        sixel_encoder_unref(encoder);
        return EXIT_FAILURE;
    }
    /* Wait for registration, not an elapsed-time guess. The normal setter
     * cannot change the threshold while this repair owns image state. */
    sixel_mutex_lock(&encoder->operation_mutex);
    while (encoder->operation_waiters == 0) {
        sixel_cond_wait(&encoder->operation_cond, &encoder->operation_mutex);
    }
    sixel_mutex_unlock(&encoder->operation_mutex);
    ok = probe.repair == SIXEL_FALSE &&
        encoder->sixdelta_threshold_limit == 255 &&
        sixel_encoder_operation_pending(encoder) &&
        !sixel_encoder_operation_admit_output(encoder);
    sixel_encoder_operation_leave(encoder);
    sixel_thread_join(&worker);
    ok = ok && worker.result == SIXEL_OK &&
        encoder->sixdelta_threshold_limit == 17;
    if (sixel_encoder_operation_enter(encoder, 1) == SIXEL_OK) {
        ok = ok && sixel_encoder_operation_admit_output(encoder);
        sixel_encoder_operation_leave(encoder);
    } else {
        ok = 0;
    }
    sixel_encoder_unref(encoder);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
#endif
}
