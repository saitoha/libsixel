/* SPDX-License-Identifier: MIT */
int band_batch_observe(int);

int
test_schedule_0026_batch_clock_reset(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return band_batch_observe(9);
}
