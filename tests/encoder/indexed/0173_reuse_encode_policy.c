/* SPDX-License-Identifier: MIT */
/* reuse encode policy. */
int core_reuse_observe(int);

int
test_0173_reuse_encode_policy(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return core_reuse_observe(10);
}
