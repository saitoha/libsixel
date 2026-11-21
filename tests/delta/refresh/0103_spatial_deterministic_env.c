/* SPDX-License-Identifier: MIT */
#include <stdio.h>
#include <string.h>
#include <sixel.h>
#include "src/refresh-spatial.h"

int
test_delta_refresh_0103(int argc, char **argv)
{
    sixel_refresh_spatial_t *state;
    sixel_allocator_t *allocator;
    sixel_delta_residual_t schedule;
    unsigned char rgb[64 * 64 * 3];
    int x, y, c, failed;

    (void)argc;
    (void)argv;
    state = NULL;
    allocator = NULL;
    failed = 0;
    memset(&schedule, 0, sizeof(schedule));
    schedule.mask.width = schedule.mask.height = 64;
    for (y = 0; y < 64; ++y) {
        for (x = 0; x < 64; ++x) {
            for (c = 0; c < 3; ++c)
                rgb[(y * 64 + x) * 3 + c] =
                    (unsigned char)((x / 4) % 2 ? 255 : 0);
        }
    }
    if (sixel_allocator_new(&allocator, NULL, NULL, NULL, NULL) != SIXEL_OK)
        return 1;
    /* Frame one must consume the frame-zero map even if its worker was
     * still computing when this call began. Frame five does the same for
     * the new flat map submitted at frame four. */
    if (sixel_refresh_spatial_prepare(&state, allocator, rgb, &schedule,
            1, 1, NULL, NULL) != SIXEL_OK) { failed = 1; goto end; }
    schedule.refresh_next_frame = 1;
    if (sixel_refresh_spatial_prepare(&state, allocator, rgb, &schedule,
            1, 1, NULL, NULL) != SIXEL_OK ||
            schedule.mask.tiled != 2 ||
            schedule.mask.cells[0] != UINT32_MAX) {
        failed = 1;
        goto end;
    }
    memset(rgb, 100, sizeof(rgb));
    schedule.refresh_next_frame = 4;
    if (sixel_refresh_spatial_prepare(&state, allocator, rgb, &schedule,
            1, 1, NULL, NULL) != SIXEL_OK) { failed = 1; goto end; }
    schedule.refresh_next_frame = 5;
    if (sixel_refresh_spatial_prepare(&state, allocator, rgb, &schedule,
            1, 1, NULL, NULL) != SIXEL_OK ||
            schedule.mask.tiled != 2 || schedule.mask.cells[0] != 0) {
        failed = 1;
    }
end:
    if (failed)
        fprintf(stderr, "deterministic spatial map boundary failed\n");
    sixel_refresh_spatial_free(state);
    sixel_allocator_unref(allocator);
    return failed;
}
