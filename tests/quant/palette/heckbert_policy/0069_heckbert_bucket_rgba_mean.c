/* SPDX-License-Identifier: MIT */
/* Four-channel 5bit buckets retain fractional channel means. */
int heckbert_bucket_mean_check(int scenario);

int
test_heckbert_bucket_rgba_mean(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return heckbert_bucket_mean_check(1);
}
