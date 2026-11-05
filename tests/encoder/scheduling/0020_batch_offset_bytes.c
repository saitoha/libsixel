/* SPDX-License-Identifier: MIT */
int band_batch_observe(int);

int
test_schedule_0020_batch_offset_bytes(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return band_batch_observe(3);
}
