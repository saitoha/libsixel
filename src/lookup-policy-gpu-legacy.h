/* SPDX-License-Identifier: MIT */
#ifndef LIBSIXEL_GPU_LOOKUP_LEGACY_H
#define LIBSIXEL_GPU_LOOKUP_LEGACY_H
#include <sixel.h>
#include <6cells.h>
SIXEL_INTERNAL_API SIXELSTATUS sixel_gpu_lookup_legacy_create(
    sixel_allocator_t *, sixel_lookup_policy_configuration_t const *,
    sixel_gpu_lookup_preparer_interface_t **);
#endif
