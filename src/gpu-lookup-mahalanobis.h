/* SPDX-License-Identifier: MIT */
#ifndef LIBSIXEL_GPU_LOOKUP_MAHALANOBIS_H
#define LIBSIXEL_GPU_LOOKUP_MAHALANOBIS_H
#include <sixel.h>
#include <6cells.h>
SIXEL_INTERNAL_API SIXELSTATUS sixel_gpu_lookup_mahalanobis_create(
    sixel_allocator_t *, sixel_gpu_lookup_preparer_interface_t **);
#endif
