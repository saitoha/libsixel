/* SPDX-License-Identifier: MIT */
/* independent correct-ICC and original-input CMYK32 fallback. */
#include "tests/loader/profile_format_frame_common.h"

int
test_loader_0201_profile(int argc, char **argv)
{
    return profile_format_frame(argc, argv, 1);
}
