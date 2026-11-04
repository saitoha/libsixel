/* SPDX-License-Identifier: MIT */
/* reuse allocation failure. */
int core_reuse_observe(int);

int
test_0182_reuse_allocation_failure(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return core_reuse_observe(19);
}
