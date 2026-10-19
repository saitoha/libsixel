/* SPDX-License-Identifier: MIT */
/* Lookup none retains exact source colors and populations. */
int heckbert_bucket_mean_check(int scenario);

int
test_heckbert_bucket_none_exact(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return heckbert_bucket_mean_check(3);
}
