/* SPDX-License-Identifier: MIT */
/* Private PNG integer-grid conversion shared with focused numeric tests. */
#ifndef LIBSIXEL_LOADER_LIBPNG_CHRM_H
#define LIBSIXEL_LOADER_LIBPNG_CHRM_H

static int
png_chrm_xy_to_xyz(double x, double y, double xyz[3])
{
    long ix;
    long iy;
    long iz;

    /* Keep the existing floating getter, including builds without the fixed
     * API. Its accepted PNG coordinates have an integer scale of 100000.
     * Check the floating range before conversion; the ordered comparisons
     * also reject NaN and infinity without an unsafe integer cast. */
    if (!(x >= 0.0 && x <= 1.0 && y > 0.0 && y <= 1.0)) {
        return 0;
    }
    ix = (long)(x * 100000.0 + 0.5);
    iy = (long)(y * 100000.0 + 0.5);
    if (ix < 0L || ix > 100000L || iy <= 0L || iy > 100000L) {
        return 0;
    }

    /* Decide the boundary on the original PNG grid. With bounded operands,
     * this signed subtraction cannot overflow even a 32-bit long. Zero Z
     * is valid; a negative numerator must retain the metadata fallback.
     * Convert to double only after classification, avoiding cancellation
     * in 1-x-y for valid boundary coordinates. */
    iz = 100000L - ix - iy;
    if (iz < 0L) {
        return 0;
    }
    xyz[0] = (double)ix / (double)iy;
    xyz[1] = 1.0;
    xyz[2] = (double)iz / (double)iy;
    return 1;
}

#endif /* LIBSIXEL_LOADER_LIBPNG_CHRM_H */
