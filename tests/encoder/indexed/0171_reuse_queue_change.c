/* SPDX-License-Identifier: MIT */
/* reuse queue change. */
int core_reuse_observe(int);

int
test_0171_reuse_queue_change(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return core_reuse_observe(8);
}
