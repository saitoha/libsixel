/* SPDX-License-Identifier: MIT */
/* Exact SIMD versus scalar score results on random inputs. */
int heckbert_score_avx2_check(int edge);
int
test_score_avx2_random(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return heckbert_score_avx2_check(0);
}
