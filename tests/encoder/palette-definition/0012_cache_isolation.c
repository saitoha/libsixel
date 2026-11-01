/* SPDX-License-Identifier: MIT */
/* Native HLS cache isolation. */
int hls_cache_contract(int);

int
test_hls_native_0012(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return hls_cache_contract(0);
}
