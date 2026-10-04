/* SPDX-License-Identifier: MIT */
/* Native HLS output reuse. */
int hls_native_contract(int);

int
test_hls_native_0011(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return hls_native_contract(10);
}
