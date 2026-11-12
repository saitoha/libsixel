/* SPDX-License-Identifier: MIT */
#ifndef LIBSIXEL_GPU_LOOKUP_H
#define LIBSIXEL_GPU_LOOKUP_H
#include <sixel.h>
#include <6cells.h>

SIXEL_INTERNAL_API SIXELSTATUS sixel_gpu_lookup_select(
    sixel_allocator_t *, sixel_lookup_policy_configuration_t const *,
    sixel_gpu_lookup_preparer_interface_t **);
SIXEL_INTERNAL_API int sixel_gpu_lookup_registered(int kind);
/* Nonzero when the provider registered for kind has its own program in
 * program_format. Only the composition layer knows this. */
SIXEL_INTERNAL_API int sixel_gpu_lookup_has_program(int kind,
    char const *program_format);
#endif
