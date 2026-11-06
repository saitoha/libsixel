/* SPDX-License-Identifier: MIT */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
#include <limits.h>
#include <string.h>
#include "src/playback-ready.h"

int
test_schedule_0030_ready_empty(int argc, char **argv)
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
    (void)a;
    if (sixel_playback_ready_pop(&q, &item) || item != b) return 1;
    return 0;
}
