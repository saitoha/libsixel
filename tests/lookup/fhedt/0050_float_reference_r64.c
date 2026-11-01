/* SPDX-License-Identifier: MIT */
/* Exact float refinement labels at R=64. */
int test_fh_refine_common(int floating, int resolution);

int
test_fh_refine_0004(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_fh_refine_common(1, 64);
}
