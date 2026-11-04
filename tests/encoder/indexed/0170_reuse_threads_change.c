/* SPDX-License-Identifier: MIT */
/* reuse threads change. */
int core_reuse_observe(int);

int
test_0170_reuse_threads_change(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return core_reuse_observe(7);
}
