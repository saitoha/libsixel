/* SPDX-License-Identifier: MIT */
/* reuse ormode change. */
int core_reuse_observe(int);

int
test_0172_reuse_ormode_change(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return core_reuse_observe(9);
}
