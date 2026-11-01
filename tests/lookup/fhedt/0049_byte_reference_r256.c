/* SPDX-License-Identifier: MIT */
/* Exact byte refinement labels at R=256. */
int test_fh_refine_common(int floating, int resolution);

int
test_fh_refine_0003(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_fh_refine_common(0, 256);
}
