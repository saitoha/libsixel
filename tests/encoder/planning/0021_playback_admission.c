/* SPDX-License-Identifier: MIT */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../_support/planner/pipeline/playback_test_common.h"

/* Admission refuses, waits and reserves within the configured capacity. */
int
test_playback_0021(int argc, char **argv)
{
#if SIXEL_ENABLE_THREADS
    playback_test_buffer_t buffer;
    sixel_output_t *output;
    sixel_encoder_t *encoder;
    sixel_playback_t *playback;
    sixel_playback_config_t config;
    sixel_playback_ticket_t first;
    sixel_playback_ticket_t second;
    sixel_playback_ticket_t probe;
    sixel_playback_stats_t stats;
    sixel_frame_t *frame;
    SIXELSTATUS status;
    int failed;

    (void)argc;
    (void)argv;
    memset(&buffer, 0, sizeof(buffer));
    output = NULL;
    playback = NULL;
    failed = 1;
    encoder = playback_test_encoder(1, "8");
    if (encoder == NULL ||
            SIXEL_FAILED(sixel_output_new(&output, playback_test_write,
                                          &buffer, NULL))) {
        goto end;
    }
    /* Loader depth must not replace the public playback admission bound. */
    if (SIXEL_FAILED(sixel_encoder_setopt(encoder,
            SIXEL_OPTFLAG_THREADING_POLICY, "greedy:B8:Q1"))) {
        goto end;
    }
    (void)sixel_playback_config_init(&config, sizeof(config));
    config.capacity = 2u;
    if (SIXEL_FAILED(sixel_playback_new(&playback, encoder, output, &config,
                                        NULL))) {
        goto end;
    }

    /* Two reservations fill the queue; a third neither waits nor blocks
     * past its timeout, and each refusal is counted. */
    if (sixel_playback_reserve(playback, SIXEL_PLAYBACK_NOWAIT, 0, &first)
            != SIXEL_OK ||
            sixel_playback_reserve(playback, SIXEL_PLAYBACK_NOWAIT, 0,
                                   &second) != SIXEL_OK) {
        fprintf(stderr, "reservations within capacity failed\n");
        goto end;
    }
    if (sixel_playback_reserve(playback, SIXEL_PLAYBACK_NOWAIT, 0, &probe)
            != SIXEL_QUEUE_FULL) {
        fprintf(stderr, "a full queue accepted a reservation\n");
        goto end;
    }
    frame = playback_test_frame(0);
    if (frame == NULL) {
        goto end;
    }
    status = sixel_playback_submit(playback, frame, SIXEL_PLAYBACK_WAIT, 20,
                                   NULL);
    sixel_frame_unref(frame);
    if (status != SIXEL_TIMEOUT) {
        fprintf(stderr, "a timed wait on a full queue returned %x\n",
                (unsigned int)status);
        goto end;
    }
    memset(&stats, 0, sizeof(stats));
    stats.struct_size = sizeof(stats);
    if (SIXEL_FAILED(sixel_playback_get_stats(playback, &stats)) ||
            stats.depth != 2u || stats.reserved != 2u ||
            stats.free_slots != 0u || stats.rejected != 2u) {
        fprintf(stderr, "full-queue statistics are inconsistent\n");
        goto end;
    }

    /* A released reservation frees its slot; a reserved slot accepts a
     * frame without waiting. */
    sixel_playback_release_reservation(playback, &second);
    frame = playback_test_frame(1);
    if (frame == NULL) {
        goto end;
    }
    status = sixel_playback_submit_reserved(playback, &first, frame, NULL);
    sixel_frame_unref(frame);
    if (status != SIXEL_OK) {
        fprintf(stderr, "a reserved submission failed\n");
        goto end;
    }
    if (SIXEL_FAILED(sixel_playback_drain(playback, -1))) {
        goto end;
    }
    stats.struct_size = sizeof(stats);
    if (SIXEL_FAILED(sixel_playback_get_stats(playback, &stats)) ||
            stats.depth != 0u || stats.submitted != 1u ||
            stats.completed != 1u || stats.free_slots != 2u) {
        fprintf(stderr, "drained statistics are inconsistent\n");
        goto end;
    }
    failed = 0;

end:
    if (playback != NULL) {
        sixel_playback_unref(playback);
    }
    sixel_output_unref(output);
    sixel_encoder_unref(encoder);
    free(buffer.bytes);
    return failed ? EXIT_FAILURE : EXIT_SUCCESS;
#else
    (void)argc;
    (void)argv;
    return 77;
#endif
}
