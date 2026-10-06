/* SPDX-License-Identifier: MIT */
/* PNG snapshot delta contract. */
int test_png_delta_recovery(int);

int
test_png_snapshot_0019(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_png_delta_recovery(0);
}
