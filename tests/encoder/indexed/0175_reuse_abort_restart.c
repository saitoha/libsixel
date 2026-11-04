/* SPDX-License-Identifier: MIT */
/* reuse abort restart. */
int core_reuse_observe(int);

int
test_0175_reuse_abort_restart(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return core_reuse_observe(12);
}
