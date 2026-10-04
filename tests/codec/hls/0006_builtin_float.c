/* SPDX-License-Identifier: MIT */
/* Native HLS builtin float. */
int hls_native_contract(int);

int
test_hls_native_0006(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return hls_native_contract(5);
}
