/* SPDX-License-Identifier: MIT */
/* reuse path. */
int core_reuse_observe(int);

int
test_0178_reuse_path(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return core_reuse_observe(15);
}
