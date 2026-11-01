/* SPDX-License-Identifier: MIT */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../_support/planner/pipeline/playback_test_common.h"

int playback_lifecycle_test(int mode);

typedef struct playback_0022_counts {
    int completed;
    int interrupted;
    int failed;
    int ordered;
    uint64_t last_sequence;
} playback_0022_counts_t;

static void
playback_0022_done(sixel_playback_result_t const *result, void *context)
{
    playback_0022_counts_t *counts;

    counts = (playback_0022_counts_t *)context;
    if (result->sequence != counts->last_sequence + 1u ||
            result->palette_sequence != (result->status == SIXEL_OK
                                          ? result->sequence : 0u)) {
        counts->ordered = 0;
    }
    counts->last_sequence = result->sequence;
    ++counts->completed;
    if (result->status == SIXEL_INTERRUPTED) {
        ++counts->interrupted;
    } else if (SIXEL_FAILED(result->status)) {
        ++counts->failed;
    }
}

/* Submit COUNT frames to a playback on ENCODER writing through WRITER. */
static sixel_playback_t *
playback_0022_start(sixel_encoder_t *encoder, sixel_output_t *output,
                    playback_0022_counts_t *counts, int count,
                    int *rejected)
{
    sixel_playback_t *playback;
    sixel_playback_config_t config;
    sixel_frame_t *frame;
    int index;

    playback = NULL;
    memset(counts, 0, sizeof(*counts));
    counts->ordered = 1;
    *rejected = 0;
    (void)sixel_playback_config_init(&config, sizeof(config));
    config.capacity = 4u;
    config.done = playback_0022_done;
    config.done_context = counts;
    if (SIXEL_FAILED(sixel_playback_new(&playback, encoder, output, &config,
                                        NULL))) {
        return NULL;
    }
    for (index = 0; index < count; ++index) {
        frame = playback_test_frame(index);
        if (frame == NULL) {
            break;
        }
        if (SIXEL_FAILED(sixel_playback_submit(playback, frame,
                                               SIXEL_PLAYBACK_WAIT, -1,
                                               NULL))) {
            ++*rejected;
        }
        sixel_frame_unref(frame);
    }
    return playback;
}

int
test_playback_0022(int argc, char **argv)
{
#if SIXEL_ENABLE_THREADS
    playback_test_buffer_t buffer;
    playback_0022_counts_t counts;
    sixel_output_t *output;
    sixel_output_t *failing;
    sixel_encoder_t *encoder;
    sixel_playback_t *playback;
    sixel_frame_t *frame;
    SIXELSTATUS status;
    int rejected;
    int failed;

    (void)argc;
    (void)argv;
    if (playback_lifecycle_test(0)) return EXIT_FAILURE;
    memset(&buffer, 0, sizeof(buffer));
    output = NULL;
    failing = NULL;
    failed = 1;
    encoder = playback_test_encoder(1, "8");
    if (encoder == NULL ||
            SIXEL_FAILED(sixel_output_new(&output, playback_test_write,
                                          &buffer, NULL)) ||
            SIXEL_FAILED(sixel_output_new(&failing,
                                          playback_test_write_fail, NULL,
                                          NULL))) {
        goto end;
    }

    /* Cancelled frames complete in order without being encoded, and an
     * attached encoder refuses ordinary calls until playback is released. */
    playback = playback_0022_start(encoder, output, &counts, 4, &rejected);
    if (playback == NULL) {
        goto end;
    }
    if (sixel_encoder_setopt(encoder, SIXEL_OPTFLAG_COLORS, "8")
            != SIXEL_BAD_ARGUMENT) {
        fprintf(stderr, "an attached encoder accepted an option\n");
        sixel_playback_unref(playback);
        goto end;
    }
    status = sixel_playback_cancel(playback);
    if (status == SIXEL_OK) {
        status = sixel_playback_drain(playback, -1);
    }
    sixel_playback_unref(playback);
    if (status != SIXEL_OK || rejected != 0 || counts.completed != 4 ||
            counts.ordered == 0 || counts.failed != 0) {
        fprintf(stderr, "cancelled playback did not complete in order\n");
        goto end;
    }
    if (SIXEL_FAILED(sixel_encoder_setopt(encoder, SIXEL_OPTFLAG_COLORS,
                                          "16"))) {
        fprintf(stderr, "a released encoder refused an option\n");
        goto end;
    }

    /* Releasing playback waits for every admitted frame. */
    playback = playback_0022_start(encoder, output, &counts, 3, &rejected);
    if (playback == NULL) {
        goto end;
    }
    sixel_playback_unref(playback);
    if (counts.completed != 3 || counts.interrupted != 0) {
        fprintf(stderr, "release did not finish admitted frames\n");
        goto end;
    }

    /* The first failure is sticky: queued frames are interrupted and later
     * admissions and drains return it. */
    playback = playback_0022_start(encoder, failing, &counts, 6, &rejected);
    if (playback == NULL) {
        goto end;
    }
    status = sixel_playback_drain(playback, -1);
    frame = playback_test_frame(9);
    if (frame == NULL) {
        sixel_playback_unref(playback);
        goto end;
    }
    if (!SIXEL_FAILED(status) ||
            !SIXEL_FAILED(sixel_playback_submit(playback, frame,
                                                SIXEL_PLAYBACK_NOWAIT, 0,
                                                NULL))) {
        fprintf(stderr, "a failed playback accepted more work\n");
        sixel_frame_unref(frame);
        sixel_playback_unref(playback);
        goto end;
    }
    sixel_frame_unref(frame);
    sixel_playback_unref(playback);
    if (counts.failed != 1 || counts.completed + rejected != 6 ||
            counts.interrupted != counts.completed - 1 ||
            counts.ordered == 0) {
        fprintf(stderr, "failure completions are inconsistent\n");
        goto end;
    }
    failed = 0;

end:
    sixel_output_unref(output);
    sixel_output_unref(failing);
    sixel_encoder_unref(encoder);
    free(buffer.bytes);
    return failed ? EXIT_FAILURE : EXIT_SUCCESS;
#else
    (void)argc;
    (void)argv;
    return 77;
#endif
}
