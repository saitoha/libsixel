/* SPDX-License-Identifier: MIT
 * Zero strength preserves every nominal byte address, including empty bins.
 */
#include <stdlib.h>
#include "src/palette-bin-histogram.h"

int
test_0012_range_zero_strength(int argc, char **argv)
{
    sixel_palette_bin_range_t range;
    unsigned int value;

    (void)argc;
    (void)argv;
    sixel_palette_bin_range_init(&range, 32u);
    for (value = 0u; value < 24u; ++value) {
        sixel_palette_bin_range_observe(&range, 0u, value >> 3u, value);
    }
    sixel_palette_bin_range_finish(&range, 1, 0u);
    for (value = 0u; value < 256u; ++value) {
        if (sixel_palette_bin_range_index(&range, 0u, value >> 3u,
                value) != value >> 3u) {
            return EXIT_FAILURE;
        }
    }
    return range.active[0] ? EXIT_FAILURE : EXIT_SUCCESS;
}
