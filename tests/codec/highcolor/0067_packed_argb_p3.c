/* SPDX-License-Identifier: MIT */
/* ARGB gamma samples convert to P3 after alpha composition. */
int test_highcolor_packed(int);

int
test_highcolor_0067(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_highcolor_packed(67);
}
