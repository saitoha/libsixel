/* SPDX-License-Identifier: MIT */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../_support/planner/pipeline/playback_test_common.h"

enum { PLAYBACK_0020_FRAMES = 10 };

typedef struct playback_0020_order {
    uint64_t last_sequence;
    int completed;
    int ordered;
} playback_0020_order_t;

static void
playback_0020_done(sixel_playback_result_t const *result, void *context)
{
    playback_0020_order_t *order;

    order = (playback_0020_order_t *)context;
    if (result->sequence != order->last_sequence + 1u ||
            result->status != SIXEL_OK) {
        order->ordered = 0;
    }
    order->last_sequence = result->sequence;
    ++order->completed;
}

/* Encode the same frame sequence directly and through playback. */
static int
playback_0020_compare(int delta, char const *threads, unsigned int capacity)
{
    playback_test_buffer_t direct;
    playback_test_buffer_t queued;
    playback_0020_order_t order;
    sixel_output_t *direct_output;
    sixel_output_t *queued_output;
    sixel_encoder_t *direct_encoder;
    sixel_encoder_t *queued_encoder;
    sixel_playback_t *playback;
    sixel_playback_config_t config;
    sixel_frame_t *frame;
    SIXELSTATUS status;
    int index;
    int failed;

    memset(&direct, 0, sizeof(direct));
    memset(&queued, 0, sizeof(queued));
    memset(&order, 0, sizeof(order));
    order.ordered = 1;
    direct_output = NULL;
    queued_output = NULL;
    playback = NULL;
    failed = 1;
    direct_encoder = playback_test_encoder(delta, threads);
    queued_encoder = playback_test_encoder(delta, threads);
    if (direct_encoder == NULL || queued_encoder == NULL ||
            SIXEL_FAILED(sixel_output_new(&direct_output,
                                          playback_test_write, &direct,
                                          NULL)) ||
            SIXEL_FAILED(sixel_output_new(&queued_output,
                                          playback_test_write, &queued,
                                          NULL))) {
        goto end;
    }
    for (index = 0; index < PLAYBACK_0020_FRAMES; ++index) {
        frame = playback_test_frame(index);
        if (frame == NULL) {
            goto end;
        }
        status = sixel_encoder_encode_frame(direct_encoder, frame,
                                            direct_output);
        sixel_frame_unref(frame);
        if (SIXEL_FAILED(status)) {
            goto end;
        }
    }

    (void)sixel_playback_config_init(&config, sizeof(config));
    config.capacity = capacity;
    config.done = playback_0020_done;
    config.done_context = &order;
    if (SIXEL_FAILED(sixel_playback_new(&playback, queued_encoder,
                                        queued_output, &config, NULL))) {
        goto end;
    }
    for (index = 0; index < PLAYBACK_0020_FRAMES; ++index) {
        frame = playback_test_frame(index);
        if (frame == NULL) {
            goto end;
        }
        status = sixel_playback_submit(playback, frame, SIXEL_PLAYBACK_WAIT,
                                       -1, NULL);
        sixel_frame_unref(frame);
        if (SIXEL_FAILED(status)) {
            goto end;
        }
    }
    if (SIXEL_FAILED(sixel_playback_drain(playback, -1))) {
        goto end;
    }
    sixel_playback_unref(playback);
    playback = NULL;
    if (order.completed != PLAYBACK_0020_FRAMES || order.ordered == 0) {
        fprintf(stderr, "completion order differs\n");
        goto end;
    }
    if (direct.size != queued.size ||
            memcmp(direct.bytes, queued.bytes, direct.size) != 0) {
        fprintf(stderr, "playback output differs: delta=%d threads=%s "
                "capacity=%u\n", delta, threads, capacity);
        goto end;
    }
    failed = 0;

end:
    if (playback != NULL) {
        sixel_playback_unref(playback);
    }
    sixel_output_unref(direct_output);
    sixel_output_unref(queued_output);
    sixel_encoder_unref(direct_encoder);
    sixel_encoder_unref(queued_encoder);
    free(direct.bytes);
    free(queued.bytes);
    return failed;
}

int
test_playback_0020(int argc, char **argv)
{
    static char const *threads[] = { "1", "8" };
    unsigned int capacity;
    int delta;
    int index;

    (void)argc;
    (void)argv;
    for (index = 0; index < 2; ++index) {
        for (delta = 0; delta < 2; ++delta) {
            for (capacity = 1u; capacity <= 4u; capacity *= 2u) {
                if (playback_0020_compare(delta, threads[index],
                                          capacity) != 0) {
                    return EXIT_FAILURE;
                }
            }
        }
    }
    return EXIT_SUCCESS;
}
