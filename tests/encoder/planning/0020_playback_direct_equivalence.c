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
playback_0020_compare(int delta, char const *threads, unsigned int capacity,
    int callbacks)
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
    direct_encoder = NULL;
    if (!callbacks) {
        direct_encoder = playback_test_encoder(delta, threads);
    }
    queued_encoder = playback_test_encoder(delta, threads);
    if ((!callbacks && direct_encoder == NULL) || queued_encoder == NULL ||
            (!callbacks && SIXEL_FAILED(sixel_output_new(&direct_output,
                                          playback_test_write, &direct,
                                          NULL))) ||
            SIXEL_FAILED(sixel_output_new(&queued_output,
                                          playback_test_write, &queued,
                                          NULL))) {
        goto end;
    }
    for (index = 0; !callbacks && index < PLAYBACK_0020_FRAMES;
            ++index) {
        frame = playback_test_small_frame(index);
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
        frame = playback_test_small_frame(index);
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
    if (callbacks && (order.completed != PLAYBACK_0020_FRAMES ||
            order.ordered == 0)) {
        fprintf(stderr, "completion order differs\n");
        goto end;
    }
    if (!callbacks && (direct.size != queued.size ||
            memcmp(direct.bytes, queued.bytes, direct.size) != 0)) {
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
    int delta;
    int threads;
    int capacity;
    int callbacks;

    if (argc != 5) {
        return EXIT_FAILURE;
    }
    delta = atoi(argv[1]);
    threads = atoi(argv[2]);
    capacity = atoi(argv[3]);
    callbacks = atoi(argv[4]);
    if ((delta != 0 && delta != 1) || (threads != 1 && threads != 8) ||
            (capacity != 1 && capacity != 2 && capacity != 4) ||
            (callbacks != 0 && callbacks != 1)) {
        return EXIT_FAILURE;
    }
    return playback_0020_compare(delta, argv[2],
        (unsigned int)capacity, callbacks) ? EXIT_FAILURE : EXIT_SUCCESS;
}
