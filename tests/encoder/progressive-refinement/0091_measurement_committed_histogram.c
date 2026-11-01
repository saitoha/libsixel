/* SPDX-License-Identifier: MIT */
int test_progressive_measure_case(int mode);

int
test_progressive_0123(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_progressive_measure_case(123);
}
