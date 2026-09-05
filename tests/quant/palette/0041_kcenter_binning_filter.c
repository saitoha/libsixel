/*
 * SPDX-License-Identifier: MIT
 *
 * Verify that K-center executes the shared hard-binning artifact while
 * preserving the palette produced by its former private dense histogram.
 */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sixel.h>

#include "src/dither.h"
#include "src/palette-kcenter.h"
#include "src/palette-plan.h"

int
test_palette_0041_kcenter_binning_filter(int argc, char **argv)
{
    static unsigned char const expected[] = {
        152u, 28u, 4u,
        12u, 240u, 136u,
        239u, 187u, 39u,
        41u, 37u, 233u
    };
    SIXELSTATUS status;
    sixel_allocator_t *allocator;
    sixel_dither_t *dither;
    sixel_palette_entries_view_t view;
    sixel_palette_build_attempt_t attempt;
    unsigned char pixels[48];

    (void)argc;
    (void)argv;
    status = SIXEL_FALSE;
    allocator = NULL;
    dither = NULL;
    memset(&view, 0, sizeof(view));
    memset(&attempt, 0, sizeof(attempt));
    pixels[0] = 7u;
    pixels[1] = 19u;
    pixels[2] = 31u;
    pixels[3] = 36u;
    pixels[4] = 72u;
    pixels[5] = 128u;
    pixels[6] = 65u;
    pixels[7] = 125u;
    pixels[8] = 225u;
    pixels[9] = 94u;
    pixels[10] = 178u;
    pixels[11] = 66u;
    pixels[12] = 123u;
    pixels[13] = 231u;
    pixels[14] = 163u;
    pixels[15] = 152u;
    pixels[16] = 28u;
    pixels[17] = 4u;
    pixels[18] = 181u;
    pixels[19] = 81u;
    pixels[20] = 101u;
    pixels[21] = 210u;
    pixels[22] = 134u;
    pixels[23] = 198u;
    pixels[24] = 239u;
    pixels[25] = 187u;
    pixels[26] = 39u;
    pixels[27] = 12u;
    pixels[28] = 240u;
    pixels[29] = 136u;
    pixels[30] = 41u;
    pixels[31] = 37u;
    pixels[32] = 233u;
    pixels[33] = 70u;
    pixels[34] = 90u;
    pixels[35] = 74u;
    pixels[36] = 99u;
    pixels[37] = 143u;
    pixels[38] = 171u;
    pixels[39] = 128u;
    pixels[40] = 196u;
    pixels[41] = 12u;
    pixels[42] = 157u;
    pixels[43] = 249u;
    pixels[44] = 109u;
    pixels[45] = 186u;
    pixels[46] = 46u;
    pixels[47] = 206u;

    status = sixel_allocator_new(&allocator, NULL, NULL, NULL, NULL);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }
    status = sixel_dither_new(&dither, 4, allocator);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }
    dither->quantize_model = SIXEL_QUANTIZE_MODEL_KCENTER;
    dither->final_merge_mode = SIXEL_FINAL_MERGE_NONE;
    sixel_palette_binning_state_init(
        &attempt.binning,
        SIXEL_PALETTE_BINNING_AUTO,
        SIXEL_PALETTE_POLICY_ORIGIN_DEFAULT);
    status = sixel_palette_binning_resolve(
        &attempt.binning,
        SIXEL_PALETTE_BINNING_HARD,
        5u,
        SIXEL_PALETTE_BINNING_GRID_UNIFORM_256,
        SIXEL_PALETTE_BINNING_KERNEL_NONE,
        SIXEL_PALETTE_BINNING_BACKEND_COMPACT_SPARSE,
        16u,
        SIXEL_PALETTE_RESOLUTION_QUANTIZER_CAPABILITY);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }
    attempt.quantize_model = SIXEL_QUANTIZE_MODEL_KCENTER;
    status = sixel_dither_initialize_with_palette_attempt(
        dither,
        pixels,
        16,
        1,
        SIXEL_PIXELFORMAT_RGB888,
        SIXEL_LARGE_AUTO,
        SIXEL_REP_AUTO,
        SIXEL_QUALITY_AUTO,
        &attempt);
    if (SIXEL_FAILED(status) ||
            attempt.quantize_model != SIXEL_QUANTIZE_MODEL_KCENTER ||
            attempt.binning.policy.phase !=
                SIXEL_PALETTE_POLICY_EXECUTED ||
            attempt.binning.policy.effective !=
                SIXEL_PALETTE_BINNING_HARD ||
            attempt.binning.bits_per_axis != 5u) {
        status = SIXEL_LOGIC_ERROR;
        goto cleanup;
    }
    status = dither->palette->vtbl->get_entries(dither->palette, &view);
    if (SIXEL_FAILED(status) || view.entry_count != 4u ||
            view.depth != 3 || view.entries == NULL ||
            memcmp(view.entries, expected, sizeof(expected)) != 0) {
        status = SIXEL_LOGIC_ERROR;
        goto cleanup;
    }
    status = SIXEL_OK;

cleanup:
    sixel_dither_unref(dither);
    sixel_allocator_unref(allocator);
    if (SIXEL_FAILED(status)) {
        fprintf(stderr, "K-center shared-binning regression failed\n");
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
