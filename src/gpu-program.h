/* SPDX-License-Identifier: MIT */
#ifndef LIBSIXEL_GPU_PROGRAM_H
#define LIBSIXEL_GPU_PROGRAM_H
#include <sixel.h>
#include <6cells.h>

struct sixel_gpu_palette_request;
/* Immutable code supplied by one dither policy. Native runtimes compose
 * it with lookup code; they never select a dither or interpret its kind. */
typedef struct sixel_gpu_dither_program {
    char const *name;
    char const *const *prefix;
    size_t prefix_count;
    char const *const *suffix;
    size_t suffix_count;
    char const *const *legacy_metal;
    size_t legacy_metal_count;
    SIXELSTATUS (*resolve)(struct sixel_gpu_palette_request *,
        struct sixel_gpu_palette_request const *);
} sixel_gpu_dither_program_t;

typedef struct sixel_gpu_program sixel_gpu_program_t;
SIXEL_INTERNAL_API SIXELSTATUS sixel_gpu_device_acquire(
    sixel_gpu_device_interface_t **);
SIXEL_INTERNAL_API SIXELSTATUS sixel_gpu_program_prepare(
    sixel_gpu_device_interface_t *, sixel_gpu_lookup_plan_interface_t *,
    sixel_gpu_dither_program_t const *, sixel_gpu_program_t **);
SIXEL_INTERNAL_API void sixel_gpu_program_destroy(sixel_gpu_program_t *);
#endif
