/* SPDX-License-Identifier: MIT */
/* Keep byte saturation and absolute noise coordinates in overlapped bands. */
int positional_query_test(int, int, int);

int
test_positional_0059(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return positional_query_test(0, 7, 0);
}
