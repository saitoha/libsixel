/* SPDX-License-Identifier: MIT */
#ifndef LIBSIXEL_GPU_RUNTIME_METAL_H
#define LIBSIXEL_GPU_RUNTIME_METAL_H
#if defined(HAVE_METAL)
#import <Metal/Metal.h>
#include <6cells.h>

/* Backend-only resource adaptation. These accessors reject foreign device
 * or buffer implementations; component clients never cast opaque storage. */
SIXEL_INTERNAL_API id<MTLDevice>
sixel_gpu_metal_device(sixel_gpu_device_interface_t *device);
SIXEL_INTERNAL_API id<MTLBuffer>
sixel_gpu_metal_buffer(sixel_gpu_device_interface_t *device,
    sixel_gpu_buffer_interface_t *buffer);

/* Record one completed command buffer in the process GPU statistics. Call
 * after waitUntilCompleted; a command that never ran adds no busy time. */
SIXEL_INTERNAL_API void
sixel_gpu_statistics_record_metal(id<MTLCommandBuffer> command);
#endif
#endif
