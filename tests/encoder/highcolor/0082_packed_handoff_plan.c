/* SPDX-License-Identifier: MIT */
/* Packed normalization requests a shared-frame copy. */
int test_highcolor_packed(int);

int
test_highcolor_0082(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_highcolor_packed(82);
}
