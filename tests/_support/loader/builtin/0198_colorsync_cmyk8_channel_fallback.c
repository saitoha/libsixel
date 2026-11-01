/* SPDX-License-Identifier: MIT */
/* Verify the public fallback after native channel-layout rejection. */
#include "tests/_support/loader/colorsync_channel_frame_common.h"

int
test_loader_0198_channels(int argc, char **argv)
{
    return colorsync_channel_frame(argc, argv, 8);
}
