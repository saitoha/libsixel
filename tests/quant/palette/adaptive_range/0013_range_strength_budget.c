/* SPDX-License-Identifier: MIT
 * Four occupied intervals in a 16-bin axis leave twelve spare addresses.
 * Quarter steps therefore permit 4, 7, 10, 13 and 16 total subdivisions.
 */
#include <stdlib.h>
#include "src/palette-bin-histogram.h"

int
test_0013_range_strength_budget(int argc, char **argv)
{
    sixel_palette_bin_range_t range;
    unsigned int strength;
    unsigned int value;
    unsigned int used;
    unsigned int bin;

    (void)argc;
    (void)argv;
    for (strength = 0u; strength <= 100u; strength += 25u) {
        sixel_palette_bin_range_init(&range, 16u);
        for (value = 0u; value < 64u; ++value) {
            sixel_palette_bin_range_observe(&range, 0u, value >> 4u,
                value);
        }
        sixel_palette_bin_range_finish(&range, 1, strength);
        used = 0u;
        for (bin = 0u; bin < 16u; ++bin) {
            used += range.cells[0][bin].divisions;
        }
        if (used != 4u + strength * 12u / 100u) {
            return EXIT_FAILURE;
        }
    }
    return EXIT_SUCCESS;
}
