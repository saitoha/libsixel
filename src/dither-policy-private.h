/* SPDX-License-Identifier: MIT */
#ifndef LIBSIXEL_DITHER_POLICY_PRIVATE_H
#define LIBSIXEL_DITHER_POLICY_PRIVATE_H
#include "dither.h"

/* Every builtin policy has this prefix. Settings are copied at prepare;
 * frame views and mutable algorithm scratch never enter this object. */
typedef struct sixel_dither_policy_object {
    sixel_dither_policy_interface_t base;
    sixel_atomic_u32_t ref;
    sixel_allocator_t *allocator;
    int method_for_scan;
    int pixelformat;
    sixel_dither_policy_options_t options;
    int prepared;
} sixel_dither_policy_object_t;

/* Temporary bridge into the existing kernels. It is stack-local to one
 * band invocation and owns none of the referenced source/history buffers. */
typedef struct sixel_dither_policy_effective {
    sixel_index_t *result;
    unsigned char *data;
    int width;
    int height;
    int band_origin;
    int output_start;
    int depth;
    unsigned char *palette;
    int method_for_scan;
    sixel_lookup_policy_interface_t *lookup_policy;
    sixel_dither_t *dither;
    sixel_dither_t context;
    int pixelformat;
} sixel_dither_policy_effective_t;

SIXEL_INTERNAL_API SIXELSTATUS
sixel_dither_policy_get_configuration(
    sixel_dither_policy_interface_t const *policy,
    sixel_dither_policy_prepare_request_t *configuration);
SIXEL_INTERNAL_API SIXELSTATUS
sixel_dither_policy_bind(sixel_dither_policy_object_t const *policy,
    sixel_dither_policy_apply_request_t const *request,
    sixel_dither_policy_effective_t *effective);
SIXEL_INTERNAL_API void
sixel_dither_policy_capture_options(sixel_dither_t const *dither,
    sixel_dither_policy_options_t *options);
SIXEL_INTERNAL_API void
sixel_dither_policy_capture_execution(sixel_dither_t const *dither,
    sixel_dither_execution_t *execution);
SIXEL_INTERNAL_API void
sixel_dither_policy_apply_options(sixel_dither_t *dither,
    sixel_dither_policy_options_t const *options);
SIXEL_INTERNAL_API void
sixel_bluenoise_prepare_options(sixel_dither_t const *dither,
    sixel_dither_policy_options_t *options);
#endif
