/* SPDX-License-Identifier: MIT */
/* reuse palette failure. */
int core_reuse_observe(int);

int
test_0176_reuse_palette_failure(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return core_reuse_observe(13);
}
