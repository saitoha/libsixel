/* SPDX-License-Identifier: MIT */
#ifndef LIBSIXEL_ENCODER_BODY_GPU_METAL_H
#define LIBSIXEL_ENCODER_BODY_GPU_METAL_H

#if defined(HAVE_METAL)
#import <Metal/Metal.h>
#include "encoder-body-gpu.h"

typedef struct sixel_body_metal_palette {
    id<MTLBuffer> rgb;
    id<MTLBuffer> count;
    id<MTLBuffer> state;
    NSUInteger count_offset;
    NSUInteger failure_offset;
} sixel_body_metal_palette_t;

SIXEL_INTERNAL_API SIXELSTATUS
sixel_body_metal_resident_new(sixel_body_gpu_t **result);
SIXEL_INTERNAL_API SIXELSTATUS
sixel_body_metal_record(sixel_body_gpu_t *context, id<MTLCommandBuffer> command,
    id<MTLBuffer> indices, sixel_body_metal_palette_t const *palette,
    unsigned int width, unsigned int height, int limited);
/* Call after the submitting command completes. Only the spelled bytes and
 * their length are shared with the host. The caller owns the returned copy. */
SIXEL_INTERNAL_API SIXELSTATUS
sixel_body_metal_copy(sixel_body_gpu_t *context, unsigned char **bytes,
                      int *size);
#endif
#endif
