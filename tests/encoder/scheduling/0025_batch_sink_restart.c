/* SPDX-License-Identifier: MIT */
int band_batch_observe(int);

int
test_schedule_0025_batch_sink_restart(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return band_batch_observe(8);
}
