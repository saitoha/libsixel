/* SPDX-License-Identifier: MIT */
/* Exact float refinement labels at R=256. */
int test_fh_refine_common(int floating, int resolution);

int
test_fh_refine_0006(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_fh_refine_common(1, 256);
}
