/* SPDX-License-Identifier: MIT */
int lso2_gpu_latest_replay(char const *, char const *);

int
test_lso2_gpu_latest_replay_t0(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return lso2_gpu_latest_replay("lso2", "delta:T0");
}
