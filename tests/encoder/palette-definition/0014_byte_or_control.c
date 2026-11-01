/* SPDX-License-Identifier: MIT */
/* Preserve HLS/OR output selection at the native and byte boundaries. */
int hls_native_contract(int);

int
test_hls_native_0014(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return hls_native_contract(11);
}
