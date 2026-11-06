/* SPDX-License-Identifier: MIT */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
#include <limits.h>
#include <string.h>
#include "src/playback-ready.h"

int
test_schedule_0029_ready_full(int argc, char **argv)
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
            !sixel_playback_ready_push(&q, b)) return 1;
    if (sixel_playback_ready_push(&q, NULL)) return 1;
    if (!sixel_playback_ready_pop(&q, &item) || item != a) return 1;
    return 0;
}
