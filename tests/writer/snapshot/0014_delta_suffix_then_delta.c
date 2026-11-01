/* SPDX-License-Identifier: MIT */
/* PNG snapshot delta contract. */
int test_png_delta_options(int);

int
test_png_snapshot_0014(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_png_delta_options(4);
}
