/* SPDX-License-Identifier: MIT */
int playback_lifecycle_test(int mode);

int
test_shared_timeline_ownership(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return playback_lifecycle_test(7);
}
