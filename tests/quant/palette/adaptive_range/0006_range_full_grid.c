/* SPDX-License-Identifier: MIT
 * A fully occupied axis retains every nominal bin exactly.
 */
#include <stdlib.h>
#include "src/palette-bin-histogram.h"

int
test_0006_range_full_grid(int argc, char **argv)
{
    sixel_palette_bin_range_t range;
    unsigned int value;

    (void)argc;
    (void)argv;
    sixel_palette_bin_range_init(&range, 64u);
    for (value = 0u; value < 256u; ++value) {
        sixel_palette_bin_range_observe(&range, 0u, value >> 2u, value);
    }
    sixel_palette_bin_range_finish(&range, 1, 100u);
    for (value = 0u; value < 256u; ++value) {
        if (sixel_palette_bin_range_index(&range, 0u, value >> 2u,
                value) != value >> 2u) {
            return EXIT_FAILURE;
        }
    }
    if (range.active[0]) {
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
