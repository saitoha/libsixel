/* SPDX-License-Identifier: MIT */
/* The generic one-channel path retains fractional means. */
int heckbert_bucket_mean_check(int scenario);

int
test_heckbert_bucket_gray_mean(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return heckbert_bucket_mean_check(2);
}
