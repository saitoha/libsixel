/* SPDX-License-Identifier: MIT */
int phased_latest_replay(char const *, char const *);

int
test_phased_latest_replay_t0(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return phased_latest_replay("phased", "delta:T0");
}
