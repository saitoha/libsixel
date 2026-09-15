/* SPDX-License-Identifier: MIT */
#ifndef LIBSIXEL_CMS_CURV_H
#define LIBSIXEL_CMS_CURV_H

#include <stddef.h>
#include <stdint.h>
#include <string.h>

/* Decode only a positive, single-gamma curveType. ICC u8Fixed8 has exactly
 * eight fewer fractional bits than s15Fixed16, so this conversion is an
 * integer representation change, not a floating-point approximation.
 * A tag may omit alignment bytes or include two zero bytes. A 15-byte tag
 * and nonzero padding remain outside the native source adapter's scope.
 * Failed recognition leaves the caller's exponent unchanged. */
static inline int
sixel_cms_curv_gamma(unsigned char const *bytes, size_t length,
                       uint32_t *gamma)
{
    uint32_t value;

    if (bytes == NULL || gamma == NULL ||
        (length != 14u && length != 16u) ||
        memcmp(bytes, "curv\0\0\0\0\0\0\0\1", 12) != 0 ||
        (length == 16u && (bytes[14] != 0u || bytes[15] != 0u))) {
        return 0;
    }
    value = ((uint32_t)bytes[12] << 16) | ((uint32_t)bytes[13] << 8);
    if (value == 0u) {
        return 0;
    }
    *gamma = value;
    return 1;
}

/* Serialize the decoded exponent, including an RGB channel permutation.
 * Copying four bytes from a curveType gamma would retain its two-byte
 * value and padding instead of a parametricCurveType s15Fixed16 exponent. */
static inline void
sixel_cms_write_gamma(unsigned char bytes[4], uint32_t gamma)
{
    bytes[0] = (unsigned char)(gamma >> 24);
    bytes[1] = (unsigned char)(gamma >> 16);
    bytes[2] = (unsigned char)(gamma >> 8);
    bytes[3] = (unsigned char)gamma;
}

#endif
