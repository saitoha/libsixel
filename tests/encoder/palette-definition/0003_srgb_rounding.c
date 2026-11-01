/* SPDX-License-Identifier: MIT */
/* Native HLS srgb rounding. */
int hls_native_contract(int);

int
test_hls_native_0003(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return hls_native_contract(2);
}
