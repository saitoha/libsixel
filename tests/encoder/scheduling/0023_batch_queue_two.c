/* SPDX-License-Identifier: MIT */
int band_batch_observe(int);

int
test_schedule_0023_batch_queue_two(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return band_batch_observe(6);
}
