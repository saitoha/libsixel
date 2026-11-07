/* SPDX-License-Identifier: MIT */
#include "tests/_support/phased/rendezvous.h"
#include <string.h>

int
test_phased_queue_sample_live(int argc, char **argv)
{
#if SIXEL_ENABLE_THREADS && SIXEL_ENCODER_PREPARE_AHEAD_SUPPORTED \
    && SIXEL_ATOMIC_U32_LOCK_FREE
    struct phased_rendezvous_fixture s;
    int ok;
    SIXELSTATUS status;
    unsigned int i;
    int attempt;
    sixel_playback_queue_sample_t sample;

    (void)argc;
    (void)argv;
    ok = 0;
    AHEAD_CHECK(!phased_rendezvous_open_portable(&s, "auto"));
    AHEAD_CHECK(phased_rendezvous_submit(&s, 0));
    AHEAD_CHECK(phased_rendezvous_wait(&s, &s.writer_entered));
    AHEAD_CHECK(phased_rendezvous_submit(&s, 1));
    AHEAD_CHECK(phased_rendezvous_wait(&s, &s.second_encoded));
    memset(&sample, 0, sizeof(sample));
    sample.struct_size = sizeof(sample);
    /* The encode hook precedes ring publication. Poll only until that
     * handoff; the stalled writer makes the expected depth stable. */
    for (attempt = 0; attempt < 1000; ++attempt) {
        status = sixel_playback_get_queue_sample(s.playback, &sample);
        if (status == SIXEL_OK && sample.depth[3] == 1) break;
        sixel_mutex_lock(&s.mutex);
        (void)sixel_cond_timedwait(&s.cond, &s.mutex, 1);
        sixel_mutex_unlock(&s.mutex);
    }
    AHEAD_CHECK(status == SIXEL_OK && sample.valid == 1);
    AHEAD_CHECK(sample.stage_count == 4 && sample.pending_palette == 0);
    AHEAD_CHECK(sample.pending_capacity == 4);
    AHEAD_CHECK(sample.pending_palette <= sample.pending_capacity);
    for (i = 0; i < 4; ++i) {
        AHEAD_CHECK(sample.capacity[i] == 2);
        AHEAD_CHECK(sample.depth[i] == (i == 3 ? 1u : 0u));
    }

    ok = 1;
end:
    phased_rendezvous_close(&s);
    return !ok;
#else
    (void)argc;
    (void)argv;
    return 77;
#endif
}
