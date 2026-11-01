/* SPDX-License-Identifier: MIT */
/* Exact byte refinement labels at R=64. */
int test_fh_refine_common(int floating, int resolution);

int
test_fh_refine_0001(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_fh_refine_common(0, 64);
}
