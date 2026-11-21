/* SPDX-License-Identifier: MIT */
/* Exact SIMD versus scalar bounds results on edge inputs. */
int heckbert_bounds_avx2_check(int edge);
int
test_bounds_avx2_edge(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return heckbert_bounds_avx2_check(1);
}
