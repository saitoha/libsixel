/* SPDX-License-Identifier: MIT */
#ifndef TESTS_LSO2_GPU_FIXTURE_H
#define TESTS_LSO2_GPU_FIXTURE_H
#include <stdio.h>
#include <string.h>
#include <sixel.h>
#include <6cells.h>
#include "src/encode-result.h"
#include "src/dither-policy-lso2-gpu.h"
#include "src/lookup-policy-ordered-gpu.h"
#include "src/lookup-policy-vptree-gpu.h"
#include "src/gpu-program.h"
#include "src/palette.h"
#include "src/frame.h"
#include "src/delta-residual.h"

#define LSO2_GPU_WIDTH 19
#define LSO2_GPU_HEIGHT 17
#define LSO2_GPU_PIXELS (LSO2_GPU_WIDTH * LSO2_GPU_HEIGHT)
#define LSO2_GPU_CHECK(condition) do { \
    if (!(condition)) { \
        fprintf(stderr, "lso2 gpu assertion line %d: %s (%s)\n", \
            __LINE__, #condition, sixel_helper_get_additional_message()); \
        goto end; \
    } \
} while (0)

typedef struct lso2_gpu_fixture {
    sixel_allocator_t *allocator;
    sixel_gpu_device_interface_t *device;
    sixel_gpu_lookup_preparer_interface_t *lookup;
    sixel_gpu_dither_preparer_interface_t *dither;
    sixel_palette_apply_interface_t *apply;
    sixel_palette_t *palette;
    sixel_wire_palette_t *wire;
    sixel_retained_plane_interface_t *plane;
    sixel_retained_read_t *read;
    sixel_frame_t *source;
    sixel_indexed_frame_t *image;
    sixel_dither_policy_prepare_request_t configuration;
    sixel_palette_apply_prepare_request_t prepare;
    sixel_palette_apply_run_request_t run;
    unsigned char pixels[LSO2_GPU_PIXELS * 3];
} lso2_gpu_fixture_t;

int lso2_gpu_batch_fault_check(int);
int lso2_gpu_batch_equivalence(unsigned int, int, int, int);
int lso2_gpu_fractional_check(int kind, int ties);
int lso2_gpu_fractional_reuse_check(int kind);
int lso2_gpu_fractional_config_check(
    sixel_lookup_policy_configuration_t const *, int);
unsigned int lso2_gpu_fractional_reference(
    sixel_lookup_policy_configuration_t const *, unsigned char const *,
    unsigned int, double const *);

SIXELSTATUS lso2_gpu_fixture_baseline(lso2_gpu_fixture_t *f, int offset);
int lso2_gpu_fixture_open(lso2_gpu_fixture_t *f, int require_gpu);
/* Open for a GPU backend without a runtime device (Vulkan): no device or
 * injected lookup, the NONE policy in gpu_direct_lookup, and the GPU
 * dither component of METHOD. Returns 77 when no such backend serves it. */
int lso2_gpu_fixture_open_direct(lso2_gpu_fixture_t *f, int method);
/* Swap in the GPU dither component of METHOD; fixed-tap components follow
 * the CPU carry limiter. */
int lso2_gpu_fixture_use_method(lso2_gpu_fixture_t *f, int method);
/* Open for METHOD with the lookup CONFIGURATION on whichever GPU backend
 * exists: an injected lookup program on a runtime device (Metal), else
 * the resolved configuration of a device-less backend (Vulkan). Returns
 * 77 when neither serves it. */
int lso2_gpu_fixture_open_lookup(lso2_gpu_fixture_t *f, int method,
    sixel_lookup_policy_configuration_t const *configuration);
SIXELSTATUS lso2_gpu_fixture_deferred(lso2_gpu_fixture_t *f);
void lso2_gpu_fixture_close(lso2_gpu_fixture_t *f);
SIXELSTATUS lso2_gpu_fixture_prepare(lso2_gpu_fixture_t *f);
SIXELSTATUS lso2_gpu_fixture_run(lso2_gpu_fixture_t *f);
#endif
