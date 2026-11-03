/* SPDX-License-Identifier: MIT */
#include <string.h>
#include "src/retained-plane-rows.h"
#include "retained_rows_common.h"

int
retained_rows_check(int neon, int width, int height, int colors,
    int pattern, int record_paint, int first, int last,
    enum retained_rows_observation observation)
{
    sixel_retained_rows_t rows;
    sixel_retained_update_stats_t stats;
    sixel_wire_color_t palette[256];
    sixel_wire_palette_view_t view;
    unsigned char rgb[270 * 5 * 3];
    unsigned char valid[270 * 5];
    unsigned char paint[256 * 3];
    unsigned char indexes[256 * 3];
    unsigned char skip[256 * 3];
    unsigned char expected_rgb[270 * 5 * 3];
    unsigned char expected_valid[270 * 5];
    unsigned char expected_paint[256 * 3];
    size_t expected_count;
    int expected_refinement;
    size_t i;
    size_t target;
    int x;
    int y;
    int component;
    unsigned int color;
    unsigned char value;

    memset(&rows, 0, sizeof(rows));
    memset(&stats, 0, sizeof(stats));
    memset(&view, 0, sizeof(view));
    memset(paint, 0, sizeof(paint));
    memset(expected_paint, 0, sizeof(expected_paint));
    for (i = 0; i < sizeof(rgb); ++i) {
        rgb[i] = expected_rgb[i] = (unsigned char)(i * 29 + 53);
    }
    for (i = 0; i < sizeof(valid); ++i) {
        valid[i] = expected_valid[i] = (unsigned char)(i % 2);
    }
    for (i = 0; i < (size_t)colors; ++i) {
        for (component = 0; component < 3; ++component) {
            palette[i].emitted_rgb[component] =
                (unsigned char)(i * (43 + component * 12) + component + 7);
        }
    }
    for (i = 0; i < (size_t)width * (size_t)height; ++i) {
        color = (unsigned int)((i * 61 + 255) % (size_t)colors);
        value = i % 3 == 0 ? 0 : (i % 3 == 1 ? 1 : 3);
        if (pattern == 0 || pattern == 4) value = 0;
        if (pattern == 1) value = 1;
        if (pattern == 2) value = 3;
        indexes[i] = value == 0 ? (unsigned char)color : 255;
        skip[i] = value;
    }
    expected_count = 0;
    expected_refinement = 0;
    for (y = first; y < last; ++y) {
        for (x = 0; x < width; ++x) {
            i = (size_t)y * (size_t)width + (size_t)x;
            if (skip[i] != 0) {
                expected_refinement |= skip[i] == 3;
                continue;
            }
            color = (unsigned int)((i * 61 + 255) % (size_t)colors);
            target = (size_t)(y + 1) * 270 + (size_t)x + 3;
            for (component = 0; component < 3; ++component) {
                expected_rgb[target * 3 + (size_t)component] =
                    (unsigned char)(color * (43 + component * 12) +
                        component + 7);
            }
            expected_valid[target] = 1;
            expected_paint[i] = 1;
            ++expected_count;
        }
    }
    view.colors = palette;
    view.count = (size_t)colors;
    rows.indexes = indexes;
    rows.skip = pattern == 4 ? NULL : skip;
    rows.rgb = rgb + (270 + 3) * 3;
    rows.valid = valid + 270 + 3;
    rows.paint = record_paint ? paint : NULL;
    rows.width = width;
    rows.stride = 270;
    sixel_retained_rows_prepare(&rows, &view);
    if (neon > 0 && !rows.neon) return 77;
    if (neon == 0) rows.neon = 0;
    sixel_retained_rows_run(&rows, first, last, &stats);
    switch (observation) {
    case RETAINED_ROWS_RGB:
        return memcmp(rgb, expected_rgb, sizeof(rgb)) != 0;
    case RETAINED_ROWS_VALID:
        return memcmp(valid, expected_valid, sizeof(valid)) != 0;
    case RETAINED_ROWS_PAINT:
        return memcmp(paint, expected_paint, sizeof(paint)) != 0;
    case RETAINED_ROWS_COUNT:
        return stats.painted_pixels != expected_count;
    case RETAINED_ROWS_REFINEMENT:
        return stats.refinement_needed != expected_refinement;
    case RETAINED_ROWS_BACKEND:
        return rows.neon != (neon > 0);
    }
    return 1;
}
