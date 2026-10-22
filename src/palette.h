/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2025 libsixel developers
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#ifndef LIBSIXEL_PALETTE_H
#define LIBSIXEL_PALETTE_H

#include <6cells.h>

#include "palette-plan.h"
#include "named-color.h"

#ifdef __cplusplus
extern "C" {
#endif

struct sixel_gamut;
SIXEL_INTERNAL_API SIXELSTATUS
sixel_palette_map_gamut(sixel_palette_t *palette,
                        struct sixel_gamut const *mapper, int colorspace,
                        int pixelformat);

struct sixel_final_merge_cluster;
typedef struct sixel_final_merge_cluster sixel_final_merge_cluster_t;

/*
 * Control how the final merge phase consolidates provisional clusters
 * into the target palette.  The Ward path merges clusters using
 * linkage costs while the remaining options keep the provisional
 * palette untouched.
 */
typedef enum sixel_final_merge_dispatch {
    SIXEL_FINAL_MERGE_DISPATCH_NONE = 0,
    SIXEL_FINAL_MERGE_DISPATCH_WARD,
} sixel_final_merge_dispatch_t;

/*
 * Telemetry describing how long each palette stage spent inside the
 * quantizer.  The orchestrator uses the aggregate to emit a single-line
 * timeline summary so operators can understand where palette/build time
 * was consumed without expanding multiple rows in the visualiser.
 */
typedef struct sixel_palette_telemetry {
    double init_ms;
    double iterate_ms;
    double merge_ms;
    double export_ms;
    unsigned int iterate_count;
    unsigned int merge_iterate_count;
    int merge_mode;
} sixel_palette_telemetry_t;

/* @classid quant/palette */
SIXEL_INTERNAL_API SIXELSTATUS
sixel_palette_factory_new(sixel_allocator_t *allocator, void **object);

/*
 * Copy attempt-local state into and out of a palette instance.  Keeping the
 * mutable copy on the instance makes concurrent palette objects independent
 * and avoids pointers whose lifetime is shorter than the build operation.
 */
SIXEL_INTERNAL_API SIXELSTATUS
sixel_palette_build_attempt_begin(
    sixel_palette_t *palette,
    sixel_palette_build_attempt_t const *attempt);

SIXEL_INTERNAL_API SIXELSTATUS
sixel_palette_build_attempt_finish(
    sixel_palette_t *palette,
    sixel_palette_build_attempt_t *attempt);

/* Private job policy; it is never a frame coordinate-space identifier. */
enum sixel_palette_output_definition {
    SIXEL_PALETTE_OUTPUT_LEGACY = SIXEL_NAMED_COLOR_SRGB,
    SIXEL_PALETTE_OUTPUT_DISPLAY_P3 = SIXEL_NAMED_COLOR_DISPLAY_P3,
    SIXEL_PALETTE_OUTPUT_ADOBE_RGB = SIXEL_NAMED_COLOR_ADOBE_RGB,
    SIXEL_PALETTE_OUTPUT_DCI_P3 = SIXEL_NAMED_COLOR_DCI_P3
};

/* Keep the installed generate request ABI unchanged. This private entry
 * admits the CPU float K-means, K-center and K-medoids families, preserves
 * all five typed coordinate spaces and forbids lossy solver fallback. */
SIXEL_INTERNAL_API SIXELSTATUS
sixel_palette_generate_extended(
    sixel_palette_t *palette,
    sixel_palette_generate_request_t const *request);

/* Preserve a coherent float view; a changed mutable byte view deliberately
 * replaces all coordinates in the supplied working format. No allocation.
 * Call before application/serialization, never concurrently with mutation. */
SIXEL_INTERNAL_API SIXELSTATUS
sixel_palette_reconcile_bytes(sixel_palette_t *palette, int pixelformat);

/* Make the byte view the only palette: round the float coordinates of the
 * supplied gamma working format to bytes and release them. No allocation. */
SIXEL_INTERNAL_API SIXELSTATUS
sixel_palette_round_to_bytes(sixel_palette_t *palette, int pixelformat);

/* Cache destination RGB101 codes without changing either working view.
 * Wire quantization does not implicitly enable palette snapping. */
SIXEL_INTERNAL_API SIXELSTATUS
sixel_palette_prepare_rgb101(sixel_palette_t *palette, int source_colorspace,
                             int output_definition);

/* Legacy output representations share palette construction. Select their
 * wire transform only here, after any output-independent palette export. */
SIXEL_INTERNAL_API SIXELSTATUS
sixel_palette_prepare_output(sixel_palette_t *palette, int source_colorspace,
                             int output_definition, int output_colorspace);

struct sixel_color_def;

/* Custom endpoints follow the same finalization and own their cached model. */
SIXEL_INTERNAL_API SIXELSTATUS
sixel_palette_prepare_output_definition(sixel_palette_t *palette,
    int source_colorspace, int output_definition, int output_colorspace,
    struct sixel_color_def const *definition);

/* Borrow immutable wire codes until the next palette initialization. */
SIXEL_INTERNAL_API SIXELSTATUS
sixel_palette_get_rgb101(sixel_palette_t const *palette,
                         unsigned char const **entries, unsigned int *count);


/* Prepare once before workers start, then borrow the HLS roundtrip model.
 * Invalidated together with other output views. No working palette mutation. */
SIXEL_INTERNAL_API SIXELSTATUS
sixel_palette_prepare_hls(sixel_palette_t *palette);

SIXEL_INTERNAL_API SIXELSTATUS
sixel_palette_get_hls_rgb101(sixel_palette_t const *palette,
                             unsigned char const **entries,
                             unsigned int *count);

/* Borrow destination RGB before wire rounding, for the HLS serializer.
 * This is an output copy, not a new working-space palette or public API. */
SIXEL_INTERNAL_API SIXELSTATUS
sixel_palette_get_output_rgb(sixel_palette_t const *palette,
                             float const **entries, unsigned int *count);

/* Cached output interpretation, independent of working coordinates. */
SIXEL_INTERNAL_API int
sixel_palette_output_is_p3(sixel_palette_t const *palette);

SIXEL_INTERNAL_API int
sixel_palette_output_is_gamma(sixel_palette_t const *palette);

/* Extended triplet conversions for native delta. Require a prepared output
 * cache; return 0 without changing out on failure. Exact aliasing is valid.
 * The inverse supports gamma/linear sRGB, SMPTE-C and Display P3. */
SIXEL_INTERNAL_API int
sixel_palette_to_output(sixel_palette_t const *palette, double out[3],
                        double const in[3], int workspace);

SIXEL_INTERNAL_API int
sixel_palette_from_output(sixel_palette_t const *palette, double out[3],
                          double const in[3], int workspace);

/* A wire candidate retains its serialized fields as well as the RGB888
 * that the retained-plane model will commit. HLS fields cannot be recovered
 * by re-encoding that rounded RGB without changing the candidate. */
typedef struct sixel_palette_emitted_color {
    int type;
    int fields[3];
    unsigned char rgb[3];
    float working[3];
} sixel_palette_emitted_color_t;

SIXEL_INTERNAL_API int
sixel_palette_decode_output(sixel_palette_t const *palette,
                             unsigned char const rgb[3], int workspace,
                             float working[3]);
SIXEL_INTERNAL_API int
sixel_palette_project_output(sixel_palette_t const *palette,
                              float const source[3], int workspace,
                              int palette_type,
                              sixel_palette_emitted_color_t *emitted);

#ifdef __cplusplus
}
#endif

#endif /* LIBSIXEL_PALETTE_H */

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
