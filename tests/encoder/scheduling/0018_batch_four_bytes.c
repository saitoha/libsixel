/* SPDX-License-Identifier: MIT */
int band_batch_observe(int);

int
test_schedule_0018_batch_four_bytes(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return band_batch_observe(1);
}
