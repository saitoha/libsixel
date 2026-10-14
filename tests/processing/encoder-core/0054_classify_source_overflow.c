/* SPDX-License-Identifier: MIT */
/* Verify classify source overflow. */
int test_classify_bounds(int);

int
test_classify_0054(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_classify_bounds(0);
}
