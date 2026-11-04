/* SPDX-License-Identifier: MIT */
/* drained rejects growth. */
int pool_reuse_observe(int);

int
test_0018_drained_rejects_growth(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return pool_reuse_observe(11);
}
