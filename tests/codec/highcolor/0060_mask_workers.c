/* SPDX-License-Identifier: MIT */
/* Parallel body serialization preserves source-mask holes. */
int test_highcolor_mask(int);

int
test_highcolor_0060(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_highcolor_mask(60);
}
