/* SPDX-License-Identifier: MIT */
/* PNG snapshot p3 frame precision. */
int hls_png_contract(int);

int
test_png_snapshot_0002(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return hls_png_contract(1);
}
