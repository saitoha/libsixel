/* SPDX-License-Identifier: MIT */
/* reuse mask. */
int core_reuse_observe(int);

int
test_0165_reuse_mask(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return core_reuse_observe(2);
}
