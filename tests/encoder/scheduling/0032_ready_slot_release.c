/* SPDX-License-Identifier: MIT */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
#include <limits.h>
#include <string.h>
#include "src/playback-ready.h"

int
test_schedule_0032_ready_slot_release(int argc, char **argv)
{
    sixel_playback_ready_t q;
    struct sixel_encoder_pipeline_slot *item, *a, *b;
    int tokens[2];

    (void)argc;
    (void)argv;
    memset(&q, 0, sizeof(q));
    a = (struct sixel_encoder_pipeline_slot *)&tokens[0];
    b = (struct sixel_encoder_pipeline_slot *)&tokens[1];
    item = b;
    if (!sixel_playback_ready_push(&q, a) ||
            !sixel_playback_ready_pop(&q, &item)) return 1;
    /* Descriptor removal cannot authorize resource reuse. */
    if (sixel_playback_ready_load(&q.released[0]) != 0) return 1;
    return 0;
}
