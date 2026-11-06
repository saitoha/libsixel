/* SPDX-License-Identifier: MIT */
#ifndef LIBSIXEL_LOOKUP_POLICY_FHEDT_GPU_H
#define LIBSIXEL_LOOKUP_POLICY_FHEDT_GPU_H
#include <sixel.h>
#include <6cells.h>
SIXEL_INTERNAL_API SIXELSTATUS sixel_gpu_lookup_fhedt_create(
    sixel_allocator_t *, sixel_lookup_policy_configuration_t const *,
    sixel_gpu_lookup_preparer_interface_t **);
#endif
