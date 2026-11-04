/* SPDX-License-Identifier: MIT */
/* reuse scratch allocations. */
int core_reuse_observe(int);

int
test_0183_reuse_scratch_allocations(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return core_reuse_observe(20);
}
