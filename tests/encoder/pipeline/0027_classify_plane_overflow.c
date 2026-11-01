/* SPDX-License-Identifier: MIT */
/* Verify classify plane overflow. */
int test_classify_bounds(int);

int
test_classify_0055(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_classify_bounds(1);
}
