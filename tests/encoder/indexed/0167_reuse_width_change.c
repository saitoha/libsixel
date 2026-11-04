/* SPDX-License-Identifier: MIT */
/* reuse width change. */
int core_reuse_observe(int);

int
test_0167_reuse_width_change(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return core_reuse_observe(4);
}
