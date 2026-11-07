/* SPDX-License-Identifier: MIT */
#include "tests/_support/phased/rendezvous.h"
#include <string.h>

int
test_phased_queue_sample_unavailable(int argc, char **argv)
{
#if SIXEL_ENABLE_THREADS
    struct phased_rendezvous_fixture s;
    int ok;
    sixel_playback_queue_sample_t sample, saved;

    (void)argc;
    (void)argv;
    ok = 0;
    AHEAD_CHECK(!phased_rendezvous_open_portable(&s, "off"));
    AHEAD_CHECK(phased_rendezvous_submit(&s, 0));
    AHEAD_CHECK(phased_rendezvous_wait(&s, &s.writer_entered));
    memset(&sample, 0x5a, sizeof(sample));
    sample.struct_size = sizeof(sample);
    memcpy(&saved, &sample, sizeof(sample));
    AHEAD_CHECK(sixel_playback_get_queue_sample(s.playback, &sample)
        == SIXEL_FALSE);
    AHEAD_CHECK(memcmp(&sample, &saved, sizeof(sample)) == 0);
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
