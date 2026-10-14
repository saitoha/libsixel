/* SPDX-License-Identifier: MIT */
int test_progressive_ladder_knots(int mode);

int
test_progressive_0132(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_progressive_ladder_knots(132);
}
