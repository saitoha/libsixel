/* SPDX-License-Identifier: MIT */
/* Axis splits order fractional means inside integer buckets. */
int heckbert_fractional_axis_check(void);

int
test_heckbert_bucket_fractional_axis(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return heckbert_fractional_axis_check();
}
