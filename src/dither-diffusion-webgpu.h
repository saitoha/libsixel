/* SPDX-License-Identifier: MIT */
#ifndef SIXEL_DITHER_DIFFUSION_WEBGPU_H
#define SIXEL_DITHER_DIFFUSION_WEBGPU_H
#include <sixel.h>
#include <6cells.h>

/* The component supplies its stencil and optional 256 x 8 coefficients.
 * Both the lookup and diffusion sources remain immutable across workers. */
typedef void (*sixel_diffusion_weights_fn)(
    sixel_dither_policy_prepare_request_t const *, float *);
SIXEL_INTERNAL_API SIXELSTATUS
sixel_diffusion_webgpu_prepare(sixel_allocator_t *,
    sixel_dither_policy_prepare_request_t const *,
    sixel_gpu_dither_prepare_request_t const *,
    sixel_gpu_dither_plan_interface_t **, char const *const *, size_t,
    sixel_diffusion_weights_fn);
#endif
