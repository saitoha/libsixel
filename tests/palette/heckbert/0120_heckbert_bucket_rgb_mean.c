/* SPDX-License-Identifier: MIT */
/* RGB buckets retain fractional population means. */
int heckbert_bucket_mean_check(int scenario);

int
test_heckbert_bucket_rgb_mean(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return heckbert_bucket_mean_check(0);
}
