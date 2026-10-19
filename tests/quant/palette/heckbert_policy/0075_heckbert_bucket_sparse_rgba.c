/* SPDX-License-Identifier: MIT */
/* Sparse four-channel statistics stay within a 5 MiB allocation budget. */
int heckbert_bucket_mean_check(int scenario);

int
test_heckbert_bucket_sparse_rgba(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return heckbert_bucket_mean_check(5);
}
