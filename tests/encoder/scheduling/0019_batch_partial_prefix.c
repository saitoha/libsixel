/* SPDX-License-Identifier: MIT */
int band_batch_observe(int);

int
test_schedule_0019_batch_partial_prefix(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return band_batch_observe(2);
}
