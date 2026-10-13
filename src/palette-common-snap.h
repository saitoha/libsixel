/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2025 libsixel developers. See `AUTHORS`.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

/*
 * Declarations for palette safe-tone snapping utilities.  Keeping the helpers
 * in a dedicated header allows quantizers to request reversible palette support
 * without pulling in the broader merge infrastructure.
 */

#ifndef LIBSIXEL_PALETTE_COMMON_SNAP_H
#define LIBSIXEL_PALETTE_COMMON_SNAP_H

#include <sixel.h>

#ifdef __cplusplus
extern "C" {
#endif

void
sixel_palette_reversible_palette(unsigned char *palette,
                                 unsigned int colors,
                                 int pixelformat);

SIXEL_INTERNAL_API void
sixel_palette_reversible_palette_float(float *palette,
                                       unsigned int colors,
                                       int pixelformat);

/* Snap typed coordinates once, then refresh their packed byte view. */
SIXEL_INTERNAL_API void
sixel_palette_snap_entries(unsigned char *entries,
                          float *entries_float32,
                          unsigned int colors,
                          int pixelformat);

/* Final typed targets; use the active destination grid when present.
 * Retain extended coordinates until blending.
 * Failure leaves every entry unchanged. No byte compatibility view is read. */
SIXEL_INTERNAL_API SIXELSTATUS
sixel_palette_snap_native(float *entries, unsigned int colors,
                           int pixelformat);

enum sixel_palette_snap_stage {
    SIXEL_PALETTE_SNAP_STAGE_FINAL_OUTPUT = 0,
    SIXEL_PALETTE_SNAP_STAGE_FINAL_MERGE_PRE,
    SIXEL_PALETTE_SNAP_STAGE_FINAL_MERGE_ITER,
    SIXEL_PALETTE_SNAP_STAGE_QUANTIZER_ITER,
    SIXEL_PALETTE_SNAP_STAGE_INITIAL_SEED,
    SIXEL_PALETTE_SNAP_STAGE_SAMPLE,
    SIXEL_PALETTE_SNAP_STAGE_APPLY
};

enum sixel_palette_snap_policy {
    SIXEL_PALETTE_SNAP_POLICY_NONE = 0,
    SIXEL_PALETTE_SNAP_POLICY_NEAREST,
    SIXEL_PALETTE_SNAP_POLICY_REVERSIBLE,
    SIXEL_PALETTE_SNAP_POLICY_AUTO
};

enum sixel_palette_snap_timing_policy {
    SIXEL_PALETTE_SNAP_TIMING_ONCE = 0,
    SIXEL_PALETTE_SNAP_TIMING_POLISH,
    SIXEL_PALETTE_SNAP_TIMING_MERGE,
    SIXEL_PALETTE_SNAP_TIMING_RESOLVE,
    SIXEL_PALETTE_SNAP_TIMING_ALL,
    SIXEL_PALETTE_SNAP_TIMING_SAMPLE,
    SIXEL_PALETTE_SNAP_TIMING_SEED,
    SIXEL_PALETTE_SNAP_TIMING_ITERATION,
    SIXEL_PALETTE_SNAP_TIMING_PREMERGE,
    SIXEL_PALETTE_SNAP_TIMING_MERGE_ITERATION,
    SIXEL_PALETTE_SNAP_TIMING_APPLY
};

typedef struct sixel_palette_snap_options {
    int policy_override;
    int policy;
    int timing_override;
    int timing;
    int approach_rate_override;
    double approach_rate;
    int channel_factor_l_override;
    double channel_factor_l;
} sixel_palette_snap_options_t;

SIXEL_INTERNAL_API void
sixel_set_palette_snap_override(
    sixel_palette_snap_options_t const *options);

int
sixel_palette_snap_is_enabled(void);

SIXEL_INTERNAL_API int
sixel_palette_snap_is_exact(void);

/* Resolve output rounding after colorspace conversion, only for rate=1. */
void
sixel_palette_snap_output(unsigned char *entries,
                         float *entries_float32,
                         unsigned int colors);

int
sixel_palette_should_snap(enum sixel_palette_snap_stage stage);

/* Typed solver doubles remain untouched when the hook is inactive. Active
 * stages share the final typed target search; byte inputs remain bounded. */
SIXEL_INTERNAL_API void
sixel_palette_snap_triple(double *components,
                          int use_reversible,
                          int pixelformat,
                          enum sixel_palette_snap_stage stage);

/* Destination and grid policy are scoped to the encoder's build override.
 * A build owns its mutable cache; another encoder cannot reuse that state. */
SIXEL_INTERNAL_API void
sixel_palette_snap_set_output(int definition, int grid_override,
                               int grid_policy);
SIXEL_INTERNAL_API int
sixel_palette_snap_destination_active(void);
SIXEL_INTERNAL_API SIXELSTATUS
sixel_palette_snap_begin(int pixelformat, int quantizer,
                          sixel_allocator_t *allocator);
/* Merge/medoid solvers use nominal byte-scaled doubles, not typed units. */
SIXEL_INTERNAL_API void
sixel_palette_snap_scaled_triple(double *components, int pixelformat,
                                  enum sixel_palette_snap_stage stage);
SIXEL_INTERNAL_API SIXELSTATUS
sixel_palette_snap_end(void);
SIXEL_INTERNAL_API SIXELSTATUS
sixel_palette_snap_status(void);
SIXEL_INTERNAL_API SIXELSTATUS
sixel_palette_snap_stage_entries(float *entries, unsigned int colors,
    int pixelformat, enum sixel_palette_snap_stage stage);

/*
 * Safe-tone lookup table used when constructing reversible palettes.  Values
 * are precomputed to keep the snapping logic branchless in callers while still
 * staying within 8-bit boundaries.
 */
static unsigned char const sixel_safe_tones[256] = {
    0,   0,   3,   3,   5,   5,   5,   8,   8,   10,  10,  10,  13,  13,  13,
    15,  15,  18,  18,  18,  20,  20,  23,  23,  23,  26,  26,  28,  28,  28,
    31,  31,  33,  33,  33,  36,  36,  38,  38,  38,  41,  41,  41,  43,  43,
    46,  46,  46,  48,  48,  51,  51,  51,  54,  54,  56,  56,  56,  59,  59,
    61,  61,  61,  64,  64,  64,  66,  66,  69,  69,  69,  71,  71,  74,  74,
    74,  77,  77,  79,  79,  79,  82,  82,  84,  84,  84,  87,  87,  89,  89,
    89,  92,  92,  92,  94,  94,  97,  97,  97,  99,  99,  102, 102, 102, 105,
    105, 107, 107, 107, 110, 110, 112, 112, 112, 115, 115, 115, 117, 117, 120,
    120, 120, 122, 122, 125, 125, 125, 128, 128, 130, 130, 130, 133, 133, 135,
    135, 135, 138, 138, 140, 140, 140, 143, 143, 143, 145, 145, 148, 148, 148,
    150, 150, 153, 153, 153, 156, 156, 158, 158, 158, 161, 161, 163, 163, 163,
    166, 166, 166, 168, 168, 171, 171, 171, 173, 173, 176, 176, 176, 179, 179,
    181, 181, 181, 184, 184, 186, 186, 186, 189, 189, 191, 191, 191, 194, 194,
    194, 196, 196, 199, 199, 199, 201, 201, 204, 204, 204, 207, 207, 209, 209,
    209, 212, 212, 214, 214, 214, 217, 217, 217, 219, 219, 222, 222, 222, 224,
    224, 227, 227, 227, 230, 230, 232, 232, 232, 235, 235, 237, 237, 237, 240,
    240, 242, 242, 242, 245, 245, 245, 247, 247, 250, 250, 250, 252, 252, 255,
    255
};

#define sixel_palette_reversible_value(sample) \
    sixel_safe_tones[(sample) >= 255 ? 255 : (sample)]

#ifdef __cplusplus
}
#endif

#endif /* LIBSIXEL_PALETTE_COMMON_SNAP_H */

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
