/* SPDX-License-Identifier: MIT */
/* reused workspace cleanup. */
int pool_reuse_observe(int);

int
test_0017_reused_workspace_cleanup(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return pool_reuse_observe(10);
}
