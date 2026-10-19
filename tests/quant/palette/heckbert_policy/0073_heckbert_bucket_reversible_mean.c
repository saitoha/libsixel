/* SPDX-License-Identifier: MIT */
/* Reversible bucket means retain the reversible tone grid. */
int heckbert_bucket_mean_check(int scenario);

int
test_heckbert_bucket_reversible_mean(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return heckbert_bucket_mean_check(4);
}
