/* SPDX-License-Identifier: MIT */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
#include <stdlib.h>
#include <string.h>
#include <sixel.h>
#include "src/encoder.h"

#if SIXEL_ENABLE_THREADS
typedef struct progressive_admission_probe {
    sixel_encoder_t *encoder;
    sixel_progressive_t *context;
    sixel_progressive_stats_t stats;
    SIXELSTATUS status;
    int register_update;
} progressive_admission_probe_t;

static int
progressive_admission_worker(void *data)
{
    progressive_admission_probe_t *probe;
    sixel_progressive_budget_t budget;

    probe = data;
    memset(&budget, 0, sizeof(budget));
    budget.struct_size = sizeof(budget);
    budget.max_packet_bytes = 4096;
    budget.max_pixel_visits = budget.max_distance_evaluations = 1;
    memset(&probe->stats, 0, sizeof(probe->stats));
    probe->stats.struct_size = sizeof(probe->stats);
    probe->status = sixel_progressive_step(probe->context, &budget, NULL,
                                             &probe->stats);
    if (probe->register_update) {
        return sixel_encoder_set_6delta_threshold_limit(probe->encoder, 17);
    }
    return 0;
}
#endif

int
test_progressive_0100(int argc, char **argv)
{
#if SIXEL_ENABLE_THREADS
    sixel_encoder_t *encoder;
    sixel_progressive_t *context;
    sixel_progressive_config_t config;
    sixel_thread_t first;
    sixel_thread_t second;
    progressive_admission_probe_t busy;
    progressive_admission_probe_t pending;
    int active;
    int launched;
    int ok;
#endif

    (void)argc;
    (void)argv;
#if !SIXEL_ENABLE_THREADS
    return 77;
#else
    encoder = NULL;
    context = NULL;
    active = launched = ok = 0;
    memset(&busy, 0, sizeof(busy));
    memset(&pending, 0, sizeof(pending));
    if (sixel_encoder_new(&encoder, NULL) != SIXEL_OK ||
            sixel_encoder_setopt(encoder, SIXEL_OPTFLAG_UPDATE_POLICY,
                "delta") != SIXEL_OK ||
            sixel_progressive_config_init(&config, sizeof(config)) !=
                SIXEL_OK ||
            sixel_progressive_new(&context, encoder, &config, NULL) !=
                SIXEL_OK ||
            sixel_encoder_operation_enter(encoder,
                SIXEL_ENCODER_OPERATION_REPAIR) != SIXEL_OK) {
        goto end;
    }
    active = 1;
    busy.encoder = pending.encoder = encoder;
    busy.context = pending.context = context;
    busy.register_update = 1;
    if (sixel_thread_create(&first, progressive_admission_worker, &busy)) {
        goto end;
    }
    launched = 1;
    /* Registration is the barrier. No source is bound: a declined admission
     * must return only admission facts without reading the source state. */
    sixel_mutex_lock(&encoder->operation_mutex);
    while (encoder->operation_waiters == 0) {
        sixel_cond_wait(&encoder->operation_cond, &encoder->operation_mutex);
    }
    sixel_mutex_unlock(&encoder->operation_mutex);
    if (sixel_thread_create(&second, progressive_admission_worker, &pending)) {
        goto end;
    }
    sixel_thread_join(&second);
    ok = busy.status == SIXEL_OK && pending.status == SIXEL_OK &&
        busy.stats.outcome == SIXEL_PROGRESSIVE_YIELDED &&
        pending.stats.outcome == SIXEL_PROGRESSIVE_YIELDED &&
        busy.stats.yield_reason == SIXEL_PROGRESSIVE_YIELD_BUSY &&
        pending.stats.yield_reason == SIXEL_PROGRESSIVE_YIELD_UPDATE_PENDING &&
        !busy.stats.state_snapshot_valid &&
        !pending.stats.state_snapshot_valid &&
        busy.stats.source_generation == 0 &&
        pending.stats.history_revision == 0 &&
        busy.stats.pixel_visits == 0 && pending.stats.packet_bytes == 0;
end:
    if (active) {
        sixel_encoder_operation_leave(encoder);
    }
    if (launched) {
        sixel_thread_join(&first);
        ok = ok && first.result == SIXEL_OK;
    }
    sixel_progressive_unref(context);
    sixel_encoder_unref(encoder);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
#endif
}
