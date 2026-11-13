/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2026 libsixel developers. See `AUTHORS`.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef LIBSIXEL_GPU_PALETTE_H
#define LIBSIXEL_GPU_PALETTE_H

#include <stddef.h>
#include "delta-residual.h"
#include "gpu-program.h"

#include <sixel.h>
#include <6cells.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SIXEL_GPU_PALETTE_AUTO_THRESHOLD_DEFAULT 262144U

/* Per-request fault/observation seam for backend integration tests. It is
 * never configured by an option or environment variable. The caller owns
 * this record through the synchronous request; ordinary requests use NULL.
 * Faults model allocation, encoder creation and completion errors. */
typedef struct sixel_gpu_palette_test_hooks {
    int fault;
    int entered;
    int matrix_supported;
    /* 1 direct, 2 gemm, 3 block, 4 grid, 5 order, 6 graph,
     * 7 FHEDT, 8 bit LUT, 9 injected RGB8 program. */
    int kernel;
    int rebuild;
    int published;
    /* Apply-command counters exclude builder passes in the first command.
     * fault_after_chunks selects a zero-based apply command for faults 2/3. */
    unsigned int fault_after_chunks;
    unsigned int submitted_chunks;
    unsigned int completed_chunks;
    unsigned int peak_inflight;
    unsigned int notified_rows;
    size_t retained_rgb_upload_bytes;
    size_t retained_valid_upload_bytes;
    /* Physical FHEDT table format, independent of requested refinement. */
    size_t fhedt_table_entry_bytes;
    size_t lookup_table_bytes;
} sixel_gpu_palette_test_hooks_t;

/*
 * PaletteApply GPU request.
 *
 * The fused consumer applies positional controls, invokes an injected lookup
 * program, and materializes indexed pixels and delta decisions. Lookup plans
 * own their shader, construction and workspace; this request carries only an
 * opaque prepared program. The enum/configuration fields remain for the
 * explicit legacy adapter and standalone compatibility entry point. Admission
 * rejects unsupported inputs before output; a prepared execution does not
 * retry a failed GPU frame on the CPU.
 */
typedef struct sixel_gpu_palette_request {
    int policy;
    sixel_gpu_program_t *program;
    sixel_gpu_palette_test_hooks_t *test_hooks;
    size_t auto_threshold;
    sixel_index_t *dest;
    unsigned char const *pixels;
    size_t pixel_count;
    int width;
    int height;
    int pixelformat;
    unsigned char const *palette;
    size_t palette_size;
    int palette_depth;
    int ncolors;
    int lut_policy;
    /* Resolved FHEDT query semantics. CPU construction controls do not
     * affect GPU ownership; a missing snapshot is never guessed. */
    int fhedt_configured;
    int fhedt_resolution;
    int fhedt_refine;
    int fhedt_use_dist2;
    int fhedt_use_cache;
    double fhedt_weights[3];
    int method_for_diffuse;
    int method_for_scan;
    int has_parallel_bands;
    /* Optional same-thread publication of a completed row prefix. Buffers
     * and context live until this synchronous request returns, including
     * draining after callback failure/cancel. The callback must not reenter
     * GPU execution: the backend retains its serialized workspace lease.
     * Zero chunk height or a legacy program retains whole-frame dispatch. */
    int rows_per_chunk;
    SIXELSTATUS (*rows_ready)(void *context, int rows);
    void *rows_context;
    int (*cancel)(void *context);
    void *cancel_context;
    unsigned char const *transparent_mask;
    size_t transparent_mask_size;
    int transparent_keycolor;
    /*
     * sixdelta_enabled says the caller wants 6delta keeps for this frame;
     * has_6delta_accumulation says this request can actually produce them.
     * Missing history must not silently turn a requested keep into a paint.
     */
    int sixdelta_enabled;
    int has_6delta_accumulation;
    /* Optional owner-pinned canvas storage on the prepared device. CPU views
     * remain available for validation; native buffers replace per-frame upload.
     * The read lease outlives the request and every queued command. */
    sixel_gpu_device_interface_t *retained_device;
    sixel_gpu_buffer_interface_t *retained_rgb;
    sixel_gpu_buffer_interface_t *retained_valid;
    unsigned char const *accumulation_pixels;
    size_t accumulation_pixels_size;
    /* Tightly packed retained canvas and this frame's origin within it.
     * Zero dimensions retain the standalone, frame-sized legacy view.
     * Without native storage only the frame rectangle is uploaded. */
    int accumulation_width;
    int accumulation_height;
    int accumulation_origin_x;
    int accumulation_origin_y;
    unsigned char const *accumulation_valid_mask;
    size_t accumulation_valid_mask_size;
    int accumulation_keycolor;
    unsigned int sixdelta_threshold;
    int sixdelta_metric;
    int sixdelta_compare;
    sixel_delta_mask_t const *residual_mask;
    /* Optional emitted RGB888 candidates for post-lookup keep only.
     * NULL preserves standalone application of the supplied palette. */
    unsigned char const *sixdelta_comparison_palette;
    size_t sixdelta_comparison_palette_size;
    unsigned char *accumulation_result_mask;
    size_t accumulation_result_mask_size;
    /* Resolved with the selected arithmetic method's registry defaults. */
    int positional_strength_override;
    float positional_strength;
    int bluenoise_strength_override;
    float bluenoise_strength;
    int bluenoise_phase_override;
    int bluenoise_phase_x;
    int bluenoise_phase_y;
    int bluenoise_seed_override;
    int bluenoise_seed;
    int bluenoise_channel_override;
    int bluenoise_channel_rgb;
    int bluenoise_size_override;
    int bluenoise_size;
    int bluenoise_gradient_factor_override;
    float bluenoise_gradient_factor;
    unsigned char const *bluenoise_gradient_map;
    size_t bluenoise_gradient_map_size;
    int bluenoise_gradient_width;
    int bluenoise_gradient_height;
} sixel_gpu_palette_request_t;

/* Internal geometry policy: avoid submission overhead on small rasters. */
SIXEL_INTERNAL_API int
sixel_gpu_palette_rows_per_chunk(int width, int height);

/* Copy only query semantics; CPU grid ownership controls are not GPU keys. */
SIXEL_INTERNAL_API void
sixel_gpu_palette_set_lookup_configuration(sixel_gpu_palette_request_t *request,
    sixel_lookup_policy_configuration_t const *configuration);

SIXEL_INTERNAL_API int
sixel_gpu_palette_fhedt_is_supported(
    sixel_gpu_palette_request_t const *request);

/*
 * Resolve the AUTO cutoff independently of backend availability.  Keeping
 * this seam observable lets registry migrations preserve the historical
 * strtoul() contract before GPU dispatch policy is changed.
 */
SIXEL_INTERNAL_API size_t
sixel_gpu_palette_auto_threshold(void);

/* Component plans resolve availability before output. A prepared dispatch
 * neither rereads options nor retries a failed GPU frame on the CPU. */
SIXEL_INTERNAL_API int
sixel_gpu_palette_plan_available(void);
/* Nonzero when the current GPU device carries the program of lookup policy
 * kind (one of SIXEL_LUT_POLICY_*), so a plan for it can be prepared. */
SIXEL_INTERNAL_API int
sixel_gpu_palette_lookup_available(int kind);
SIXEL_INTERNAL_API SIXELSTATUS
sixel_gpu_palette_apply_prepared(sixel_gpu_palette_request_t const *request);

SIXEL_INTERNAL_API SIXELSTATUS
sixel_gpu_palette_apply(sixel_gpu_palette_request_t const *request);

/*
 * Return whether the GPU policy owns the palette-apply stage before CPU dither
 * workers are scheduled.  FORCE claims the stage even when the final request
 * shape may fail, because FORCE reports that error instead of falling back to
 * the CPU path.
 */
SIXEL_INTERNAL_API int
sixel_gpu_palette_policy_claims_apply_stage(int gpu_policy,
                                            size_t auto_threshold,
                                            int lut_policy,
                                            int method_for_diffuse,
                                            int method_for_scan,
                                            size_t pixel_count);

#ifdef __cplusplus
}
#endif

#endif /* LIBSIXEL_GPU_PALETTE_H */

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
