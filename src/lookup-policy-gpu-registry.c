/* SPDX-License-Identifier: MIT */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
#include <string.h>
#include "lookup-policy-gpu.h"
#include "lookup-policy-none-gpu.h"
#include "lookup-policy-delaunay-gpu.h"
#include "lookup-policy-ordered-gpu.h"
#include "lookup-policy-vptree-gpu.h"
#include "lookup-policy-rbc-gpu.h"
#include "lookup-policy-mahalanobis-gpu.h"
#include "lookup-policy-certlut-gpu.h"
#include "lookup-policy-eytzinger-gpu.h"
#include "lookup-policy-gridlut-gpu.h"
#include "lookup-policy-blockcull-gpu.h"
#include "lookup-policy-5bit-gpu.h"
#include "lookup-policy-6bit-gpu.h"
#include "lookup-policy-fhedt-gpu.h"

/* Composition is the only layer that knows concrete lookup constructors.
 * Independent providers extend this table; execution does not change. */
/* formats lists the program formats a provider has its own program for;
 * the composition root admits a backend only for those providers. */
#define GPU_METAL 1
#define GPU_VULKAN 2
static struct {
    int kind;
    int formats;
    SIXELSTATUS (*create)(sixel_allocator_t *,
        sixel_gpu_lookup_preparer_interface_t **);
} const gpu_lookup_registrations[] = {
    { SIXEL_LUT_POLICY_NONE, GPU_METAL | GPU_VULKAN,
      sixel_gpu_lookup_none_create },
    { SIXEL_LUT_POLICY_DELAUNAY, GPU_METAL | GPU_VULKAN,
      sixel_gpu_lookup_delaunay_create },
    { SIXEL_LUT_POLICY_ORDERED, GPU_METAL | GPU_VULKAN,
      sixel_gpu_lookup_ordered_create },
    { SIXEL_LUT_POLICY_VPTREE, GPU_METAL | GPU_VULKAN,
      sixel_gpu_lookup_vptree_create },
    { SIXEL_LUT_POLICY_RBC, GPU_METAL | GPU_VULKAN,
      sixel_gpu_lookup_rbc_create },
    { SIXEL_LUT_POLICY_MAHALANOBIS, GPU_METAL | GPU_VULKAN,
      sixel_gpu_lookup_mahalanobis_create },
    { SIXEL_LUT_POLICY_CERTLUT, GPU_METAL | GPU_VULKAN,
      sixel_gpu_lookup_certlut_create },
    { SIXEL_LUT_POLICY_EYTZINGER, GPU_METAL | GPU_VULKAN,
      sixel_gpu_lookup_eytzinger_create },
    { SIXEL_LUT_POLICY_GRIDLUT, GPU_METAL | GPU_VULKAN,
      sixel_gpu_lookup_gridlut_create },
    { SIXEL_LUT_POLICY_BLOCKCULL, GPU_METAL | GPU_VULKAN,
      sixel_gpu_lookup_blockcull_create }
};

/* These providers retain their own resolved settings, without exposing
 * configuration-specific branches to dither or palette application. */
static struct {
    int kind;
    int formats;
    SIXELSTATUS (*create)(sixel_allocator_t *,
        sixel_lookup_policy_configuration_t const *,
        sixel_gpu_lookup_preparer_interface_t **);
} const gpu_configured_registrations[] = {
    { SIXEL_LUT_POLICY_5BIT, GPU_METAL | GPU_VULKAN,
      sixel_gpu_lookup_5bit_create },
    { SIXEL_LUT_POLICY_6BIT, GPU_METAL | GPU_VULKAN,
      sixel_gpu_lookup_6bit_create },
    { SIXEL_LUT_POLICY_FHEDT, GPU_METAL | GPU_VULKAN,
      sixel_gpu_lookup_fhedt_create }
};

int
sixel_gpu_lookup_registered(int kind)
{
    size_t i;

    for (i = 0; i < sizeof(gpu_lookup_registrations) /
            sizeof(*gpu_lookup_registrations); ++i) {
        if (gpu_lookup_registrations[i].kind == kind) return 1;
    }
    for (i = 0; i < sizeof(gpu_configured_registrations) /
            sizeof(*gpu_configured_registrations); ++i) {
        if (gpu_configured_registrations[i].kind == kind) return 1;
    }
    return 0;
}

int
sixel_gpu_lookup_has_program(int kind, char const *program_format)
{
    size_t i;
    int format;

    if (program_format == NULL) return 0;
    format = strcmp(program_format, "metal.lookup-rgb8.v1") == 0 ? GPU_METAL
        : strcmp(program_format, "vulkan.lookup-rgb8.v1") == 0 ? GPU_VULKAN
        : 0;
    for (i = 0; i < sizeof(gpu_lookup_registrations) /
            sizeof(*gpu_lookup_registrations); ++i) {
        if (gpu_lookup_registrations[i].kind == kind) {
            return (gpu_lookup_registrations[i].formats & format) != 0;
        }
    }
    for (i = 0; i < sizeof(gpu_configured_registrations) /
            sizeof(*gpu_configured_registrations); ++i) {
        if (gpu_configured_registrations[i].kind == kind) {
            return (gpu_configured_registrations[i].formats & format) != 0;
        }
    }
    return 0;
}

SIXELSTATUS
sixel_gpu_lookup_select(sixel_allocator_t *allocator,
    sixel_lookup_policy_configuration_t const *configuration,
    sixel_gpu_lookup_preparer_interface_t **result)
{
    size_t i;

    if (configuration == NULL || result == NULL) return SIXEL_BAD_ARGUMENT;
    *result = NULL;
    for (i = 0; i < sizeof(gpu_lookup_registrations) /
            sizeof(*gpu_lookup_registrations); ++i) {
        if (gpu_lookup_registrations[i].kind == configuration->kind) {
            return gpu_lookup_registrations[i].create(allocator, result);
        }
    }
    for (i = 0; i < sizeof(gpu_configured_registrations) /
            sizeof(*gpu_configured_registrations); ++i) {
        if (gpu_configured_registrations[i].kind == configuration->kind) {
            return gpu_configured_registrations[i].create(allocator,
                configuration, result);
        }
    }
    return SIXEL_FEATURE_ERROR;
}
