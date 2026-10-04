/* SPDX-License-Identifier: MIT */
/* Native HLS cache invalidation. */
int hls_cache_contract(int);

int
test_hls_native_0013(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return hls_cache_contract(1);
}
