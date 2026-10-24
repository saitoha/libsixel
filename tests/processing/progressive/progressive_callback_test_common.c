/* SPDX-License-Identifier: MIT */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
#include <stdlib.h>
#include <string.h>
#include <sixel.h>
#include "src/encoder.h"

/* The loader and frame worker can both poll cancellation. The probe's own
 * counters therefore need synchronization independently of encoder state. */
typedef struct callback_probe {
    sixel_encoder_t *encoder;
    sixel_thread_identity_t caller;
    sixel_mutex_t mutex;
    int calls;
    int worker_calls;
    int valid;
} callback_probe_t;

static int
progressive_cancel_probe(void *context)
{
    callback_probe_t *probe;
    SIXELSTATUS status;

    probe = context;
    status = sixel_encoder_set_6delta_threshold_limit(probe->encoder, 17);
#if SIXEL_ENABLE_THREADS
    sixel_mutex_lock(&probe->mutex);
#endif
    ++probe->calls;
    if (!sixel_thread_identity_equal(probe->caller,
                                      sixel_thread_identity_current())) {
        ++probe->worker_calls;
    }
    probe->valid = probe->valid && status == SIXEL_RUNTIME_ERROR;
#if SIXEL_ENABLE_THREADS
    sixel_mutex_unlock(&probe->mutex);
#endif
    return 0;
}

int
test_progressive_callback_mode(char const *path, int worker)
{
    callback_probe_t probe;
    SIXELSTATUS status;
    int ok;

#if !SIXEL_ENABLE_THREADS
    if (worker) {
        return 77;
    }
#endif
    memset(&probe, 0, sizeof(probe));
    probe.caller = sixel_thread_identity_current();
    probe.valid = 1;
    if (sixel_encoder_new(&probe.encoder, NULL) != SIXEL_OK) {
        return EXIT_FAILURE;
    }
#if SIXEL_ENABLE_THREADS
    if (sixel_mutex_init(&probe.mutex) != SIXEL_OK) {
        sixel_encoder_unref(probe.encoder);
        return EXIT_FAILURE;
    }
#endif
    ok = sixel_encoder_setopt(probe.encoder, SIXEL_OPTFLAG_THREADING_POLICY,
                              worker ? "greedy:B4" : "greedy:B1")
        == SIXEL_OK &&
        sixel_encoder_setopt(probe.encoder, SIXEL_OPTFLAG_LOADERS,
                              "builtin:cms_engine=none!") == SIXEL_OK &&
        sixel_encoder_setopt(probe.encoder, SIXEL_OPTFLAG_IGNORE_DELAY, NULL)
            == SIXEL_OK &&
        sixel_encoder_setopt(probe.encoder, SIXEL_OPTFLAG_GPU_POLICY, "off")
            == SIXEL_OK &&
        sixel_encoder_setopt(probe.encoder, SIXEL_OPTFLAG_OUTFILE,
#if defined(_WIN32)
                              "NUL"
#else
                              "/dev/null"
#endif
                              ) == SIXEL_OK &&
        sixel_encoder_set_cancel_callback(probe.encoder,
            progressive_cancel_probe, &probe) == SIXEL_OK;
    if (ok) {
        status = sixel_encoder_encode(probe.encoder, path);
        ok = status == SIXEL_OK && probe.valid && probe.calls > 0 &&
            probe.encoder->sixdelta_threshold_limit == 255 &&
            (worker ? probe.worker_calls > 0 : probe.worker_calls == 0);
    }
#if SIXEL_ENABLE_THREADS
    sixel_mutex_destroy(&probe.mutex);
#endif
    sixel_encoder_unref(probe.encoder);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
