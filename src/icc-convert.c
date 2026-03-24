/*
 * SPDX-License-Identifier: MIT
 *
 * Compatibility wrappers for historical ICC conversion entry points.
 *
 * New code should call cms.c helpers directly.
 */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include "icc-convert.h"
#include "icc-convert-internal.h"

#include "cms.h"

int
sixel_icc_convert_profile_to_srgb_internal(unsigned char *pixels,
                                           int width,
                                           int height,
                                           int pixelformat,
                                           sixel_cms_profile_t *src_profile)
{
    return sixel_cms_convert_profile_to_srgb(pixels,
                                             width,
                                             height,
                                             pixelformat,
                                             src_profile);
}

int
sixel_icc_convert_to_srgb_with_pixelformat(
    unsigned char *pixels,
    int width,
    int height,
    int pixelformat,
    unsigned char const *profile,
    size_t profile_length)
{
    return sixel_cms_convert_to_srgb_with_profile_bytes(pixels,
                                                        width,
                                                        height,
                                                        pixelformat,
                                                        profile,
                                                        profile_length);
}

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
