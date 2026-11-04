/* SPDX-License-Identifier: MIT */
/* reuse palette. */
int core_reuse_observe(int);

int
test_0164_reuse_palette(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return core_reuse_observe(1);
}
