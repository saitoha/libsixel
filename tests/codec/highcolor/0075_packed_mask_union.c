/* SPDX-License-Identifier: MIT */
/* Interpreted zero alpha and side holes are both omitted. */
int test_highcolor_packed(int);

int
test_highcolor_0075(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_highcolor_packed(75);
}
