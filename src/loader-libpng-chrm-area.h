/* SPDX-License-Identifier: MIT */
/* Private exact primary classification, after PNG coordinate validation. */
#ifndef LIBSIXEL_LOADER_LIBPNG_CHRM_AREA_H
#define LIBSIXEL_LOADER_LIBPNG_CHRM_AREA_H

#include <stdint.h>

static int64_t
png_chrm_primary_area(double red_x, double red_y,
                      double green_x, double green_y,
                      double blue_x, double blue_y)
{
    int64_t grid[6];
    int64_t area;

    /* The caller has validated all coordinates on the 100000-scale grid.
     * Recover the same integers before classifying collinearity. Products
     * can reach 1e10, so conversion must precede arithmetic even on systems
     * with a 32-bit long. The uncoupled subtraction bound is 2e10. */
    grid[0] = (int64_t)(red_x * 100000.0 + 0.5);
    grid[1] = (int64_t)(red_y * 100000.0 + 0.5);
    grid[2] = (int64_t)(green_x * 100000.0 + 0.5);
    grid[3] = (int64_t)(green_y * 100000.0 + 0.5);
    grid[4] = (int64_t)(blue_x * 100000.0 + 0.5);
    grid[5] = (int64_t)(blue_y * 100000.0 + 0.5);
    area = (grid[0] - grid[4]) * (grid[3] - grid[5]) -
           (grid[2] - grid[4]) * (grid[1] - grid[5]);
    return area;
}

#endif /* LIBSIXEL_LOADER_LIBPNG_CHRM_AREA_H */
