/* SPDX-License-Identifier: MIT */
int test_progressive_predict_case(int mode);

int
test_progressive_0126(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_progressive_predict_case(126);
}
