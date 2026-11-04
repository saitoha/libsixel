/* SPDX-License-Identifier: MIT */
/* A long leading gap preserves limited splitting before paint. */
int test_reserved_run(int, int, int, int, char const *);

int
test_encode_run_0123(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_reserved_run(1, 1, 0, 256, "!255??@");
}
