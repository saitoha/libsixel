/* SPDX-License-Identifier: MIT */
/* drained rejects push. */
int pool_reuse_observe(int);

int
test_0012_drained_rejects_push(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return pool_reuse_observe(5);
}
