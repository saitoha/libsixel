/* SPDX-License-Identifier: MIT */
/* drain preserves error. */
int pool_reuse_observe(int);

int
test_0016_drain_preserves_error(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return pool_reuse_observe(9);
}
