/* SPDX-License-Identifier: MIT */
/* Native HLS srgb centroid. */
int hls_native_contract(int);

int
test_hls_native_0001(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return hls_native_contract(0);
}
