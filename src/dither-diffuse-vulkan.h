/* SPDX-License-Identifier: MIT */
#ifndef LIBSIXEL_DITHER_DIFFUSE_VULKAN_H
#define LIBSIXEL_DITHER_DIFFUSE_VULKAN_H
#include <sixel.h>
#include <6cells.h>

/*
 * Vulkan execution of the four-phase GPU error diffusion that the Metal
 * dither/<method>.gpu components run. Vulkan has no runtime device or
 * injected lookup program; request->direct_lookup names the resolved
 * policy, and a plan either scans the palette for the exact nearest entry
 * or queries a device FHEDT table.
 */

/* Nonzero when a usable Vulkan device can diffuse method_for_diffuse with
 * the lookup policy kind lut_policy (AUTO resolves to an exact policy). */
SIXEL_INTERNAL_API int
sixel_dither_diffuse_vulkan_serves(int method_for_diffuse, int lut_policy);

/* As above for a resolved configuration, whose FHEDT settings must also
 * be ones the device table build reproduces. */
SIXEL_INTERNAL_API int
sixel_dither_diffuse_vulkan_lookup_serves(int method_for_diffuse,
    sixel_lookup_policy_configuration_t const *configuration);

/* Prepare one plan for a configured fixed-tap or LSO2 GPU dither.
 * SIXEL_FEATURE_ERROR means unsupported before any output. */
SIXEL_INTERNAL_API SIXELSTATUS
sixel_dither_diffuse_vulkan_prepare(sixel_allocator_t *allocator,
    sixel_dither_policy_prepare_request_t const *configuration,
    sixel_gpu_dither_prepare_request_t const *request,
    sixel_gpu_dither_plan_interface_t **result);
#endif
