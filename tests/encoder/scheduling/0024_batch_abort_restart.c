/* SPDX-License-Identifier: MIT */
int band_batch_observe(int);

int
test_schedule_0024_batch_abort_restart(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return band_batch_observe(7);
}
