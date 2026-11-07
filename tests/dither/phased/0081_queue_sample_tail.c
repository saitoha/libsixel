/* SPDX-License-Identifier: MIT */
#include "tests/_support/phased/rendezvous.h"
#include <string.h>

int
test_phased_queue_sample_tail(int argc, char **argv)
{
#if SIXEL_ENABLE_THREADS && SIXEL_ENCODER_PREPARE_AHEAD_SUPPORTED \
    && SIXEL_ATOMIC_U32_LOCK_FREE
    struct phased_rendezvous_fixture s;
    int ok;
    SIXELSTATUS status;
    unsigned int i;
    int attempt;
    struct {
        sixel_playback_queue_sample_t sample;
        unsigned char tail[16];
    } extended;
    unsigned char saved[16];

    (void)argc;
    (void)argv;
    ok = 0;
    AHEAD_CHECK(!phased_rendezvous_open_portable(&s, "auto"));
    AHEAD_CHECK(phased_rendezvous_submit(&s, 0));
    AHEAD_CHECK(phased_rendezvous_wait(&s, &s.writer_entered));
    AHEAD_CHECK(phased_rendezvous_submit(&s, 1));
    AHEAD_CHECK(phased_rendezvous_wait(&s, &s.second_encoded));
    memset(&extended, 0x5a, sizeof(extended));
    extended.sample.struct_size = sizeof(extended);
    memcpy(saved, extended.tail, sizeof(saved));
    /* The encode hook precedes ring publication. Poll only until that
     * handoff; the stalled writer makes the expected depth stable. */
    for (attempt = 0; attempt < 1000; ++attempt) {
        status = sixel_playback_get_queue_sample(s.playback, &extended.sample);
        if (status == SIXEL_OK && extended.sample.depth[3] == 1) break;
        sixel_mutex_lock(&s.mutex);
        (void)sixel_cond_timedwait(&s.cond, &s.mutex, 1);
        sixel_mutex_unlock(&s.mutex);
    }
    AHEAD_CHECK(status == SIXEL_OK && extended.sample.valid == 1);
    AHEAD_CHECK(extended.sample.stage_count == 4);
    AHEAD_CHECK(extended.sample.pending_palette == 0);
    AHEAD_CHECK(extended.sample.pending_capacity == 4);
    for (i = 0; i < 4; ++i) {
        AHEAD_CHECK(extended.sample.capacity[i] == 2);
        AHEAD_CHECK(extended.sample.depth[i] == (i == 3 ? 1u : 0u));
    }
    AHEAD_CHECK(extended.sample.struct_size == sizeof(extended));
    AHEAD_CHECK(memcmp(saved, extended.tail, sizeof(saved)) == 0);
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
