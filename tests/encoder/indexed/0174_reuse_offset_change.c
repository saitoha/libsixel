/* SPDX-License-Identifier: MIT */
/* reuse offset change. */
int core_reuse_observe(int);

int
test_0174_reuse_offset_change(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return core_reuse_observe(11);
}
