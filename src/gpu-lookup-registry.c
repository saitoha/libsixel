/* SPDX-License-Identifier: MIT */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
#include "gpu-lookup.h"
#include "gpu-lookup-ordered.h"
#include "gpu-lookup-vptree.h"
#include "gpu-lookup-legacy.h"

/* Composition is the only layer that knows concrete lookup constructors.
 * Independent providers extend this table; execution does not change. */
static struct {
    int kind;
    SIXELSTATUS (*create)(sixel_allocator_t *,
        sixel_gpu_lookup_preparer_interface_t **);
} const gpu_lookup_registrations[] = {
    { SIXEL_LUT_POLICY_ORDERED, sixel_gpu_lookup_ordered_create },
    { SIXEL_LUT_POLICY_VPTREE, sixel_gpu_lookup_vptree_create }
};

int
sixel_gpu_lookup_registered(int kind)
{
    size_t i;

    for (i = 0; i < sizeof(gpu_lookup_registrations) /
            sizeof(*gpu_lookup_registrations); ++i) {
        if (gpu_lookup_registrations[i].kind == kind) return 1;
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
    /* Frozen legacy capabilities. Do not register new algorithms here. */
    switch (configuration->kind) {
    case SIXEL_LUT_POLICY_NONE:
    case SIXEL_LUT_POLICY_EYTZINGER:
    case SIXEL_LUT_POLICY_BLOCKCULL:
    case SIXEL_LUT_POLICY_GRIDLUT:
    case SIXEL_LUT_POLICY_DELAUNAY:
    case SIXEL_LUT_POLICY_FHEDT:
    case SIXEL_LUT_POLICY_5BIT:
    case SIXEL_LUT_POLICY_6BIT:
        return sixel_gpu_lookup_legacy_create(allocator, configuration, result);
    default:
        return SIXEL_FEATURE_ERROR;
    }
}
