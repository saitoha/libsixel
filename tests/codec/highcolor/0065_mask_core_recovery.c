/* SPDX-License-Identifier: MIT */
/* Low-level masks reject short buffers and clear after reuse. */
int test_highcolor_mask(int);

int
test_highcolor_0065(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_highcolor_mask(65);
}
