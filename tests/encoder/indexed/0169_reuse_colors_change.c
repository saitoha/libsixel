/* SPDX-License-Identifier: MIT */
/* reuse colors change. */
int core_reuse_observe(int);

int
test_0169_reuse_colors_change(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return core_reuse_observe(6);
}
