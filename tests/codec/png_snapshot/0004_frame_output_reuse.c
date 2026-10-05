/* SPDX-License-Identifier: MIT */
/* PNG snapshot frame output reuse. */
int hls_png_contract(int);

int
test_png_snapshot_0004(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return hls_png_contract(10);
}
