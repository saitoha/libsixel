/*
 * SPDX-License-Identifier: MIT
 */

#ifndef LIBSIXEL_ICC_CONVERT_INTERNAL_H
#define LIBSIXEL_ICC_CONVERT_INTERNAL_H

#include "cms.h"

int
sixel_icc_convert_profile_to_srgb_internal(unsigned char *pixels,
                                           int width,
                                           int height,
                                           int pixelformat,
                                           sixel_cms_profile_t *src_profile);

#endif  /* LIBSIXEL_ICC_CONVERT_INTERNAL_H */
