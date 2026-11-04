/* SPDX-License-Identifier: MIT */
/* reuse pixels. */
int core_reuse_observe(int);

int
test_0163_reuse_pixels(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return core_reuse_observe(0);
}
