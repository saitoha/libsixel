/* SPDX-License-Identifier: MIT */
#ifndef LIBSIXEL_GPU_LOOKUP_METAL_H
#define LIBSIXEL_GPU_LOOKUP_METAL_H
#include "lookup-policy.h"

/* A byte plan supplies its native implementation, never a kind for the
 * transport to interpret. Both pointers remain valid for the plan's life. */
typedef struct gpu_metal_lookup gpu_metal_lookup_t;
typedef struct gpu_metal_lookup_program {
    gpu_metal_lookup_t const *implementation;
    sixel_lookup_policy_configuration_t const *configuration;
} gpu_metal_lookup_program_t;

#if defined(HAVE_METAL)
SIXEL_INTERNAL_API gpu_metal_lookup_t const *
sixel_lookup_none_gpu_metal(void);
#else
static inline gpu_metal_lookup_t const *
sixel_lookup_none_gpu_metal(void)
{
    return NULL;
}
#endif

#if defined(HAVE_METAL)
SIXEL_INTERNAL_API gpu_metal_lookup_t const *
sixel_lookup_5bit_gpu_metal(void);
#else
static inline gpu_metal_lookup_t const *
sixel_lookup_5bit_gpu_metal(void)
{
    return NULL;
}
#endif

#if defined(HAVE_METAL)
SIXEL_INTERNAL_API gpu_metal_lookup_t const *
sixel_lookup_6bit_gpu_metal(void);
#else
static inline gpu_metal_lookup_t const *
sixel_lookup_6bit_gpu_metal(void)
{
    return NULL;
}
#endif

#if defined(HAVE_METAL)
SIXEL_INTERNAL_API gpu_metal_lookup_t const *
sixel_lookup_fhedt_gpu_metal(void);
#else
static inline gpu_metal_lookup_t const *
sixel_lookup_fhedt_gpu_metal(void)
{
    return NULL;
}
#endif

#if defined(HAVE_METAL)
SIXEL_INTERNAL_API gpu_metal_lookup_t const *
sixel_lookup_eytzinger_gpu_metal(void);
#else
static inline gpu_metal_lookup_t const *
sixel_lookup_eytzinger_gpu_metal(void)
{
    return NULL;
}
#endif

#if defined(HAVE_METAL)
SIXEL_INTERNAL_API gpu_metal_lookup_t const *
sixel_lookup_gridlut_gpu_metal(void);
#else
static inline gpu_metal_lookup_t const *
sixel_lookup_gridlut_gpu_metal(void)
{
    return NULL;
}
#endif

#if defined(HAVE_METAL)
SIXEL_INTERNAL_API gpu_metal_lookup_t const *
sixel_lookup_delaunay_gpu_metal(void);
#else
static inline gpu_metal_lookup_t const *
sixel_lookup_delaunay_gpu_metal(void)
{
    return NULL;
}
#endif

#if defined(HAVE_METAL)
SIXEL_INTERNAL_API gpu_metal_lookup_t const *
sixel_lookup_blockcull_gpu_metal(void);
#else
static inline gpu_metal_lookup_t const *
sixel_lookup_blockcull_gpu_metal(void)
{
    return NULL;
}
#endif
#endif
