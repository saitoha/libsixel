/* SPDX-License-Identifier: MIT */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
#include "dither-policy-gpu.h"
#include "dither-policy-none-gpu.h"
#include "dither-policy-a-dither-gpu.h"
#include "dither-policy-x-dither-gpu.h"
#include "dither-policy-bluenoise-gpu.h"

/* Selection belongs to this composition root, never the native executor. */
static struct {
    int method;
    sixel_gpu_dither_program_t const *program;
    SIXELSTATUS (*create)(sixel_allocator_t *, void **);
} const byte_dithers[] = {
    { SIXEL_DIFFUSE_NONE, &sixel_dither_none_gpu_program,
      sixel_dither_policy_none_gpu_new },
    { SIXEL_DIFFUSE_A_DITHER, &sixel_dither_a_dither_gpu_program,
      sixel_dither_policy_a_dither_gpu_new },
    { SIXEL_DIFFUSE_X_DITHER, &sixel_dither_x_dither_gpu_program,
      sixel_dither_policy_x_dither_gpu_new },
    { SIXEL_DIFFUSE_BLUENOISE_DITHER, &sixel_dither_bluenoise_gpu_program,
      sixel_dither_policy_bluenoise_gpu_new },
};

sixel_gpu_dither_program_t const *
sixel_gpu_dither_byte_program(int method)
{
    size_t i;

    for (i = 0; i < sizeof(byte_dithers) / sizeof(*byte_dithers); ++i)
        if (byte_dithers[i].method == method) return byte_dithers[i].program;
    return NULL;
}

SIXELSTATUS
sixel_gpu_dither_byte_create(sixel_allocator_t *allocator, int method,
    sixel_gpu_dither_preparer_interface_t **result)
{
    size_t i;

    if (result == NULL) return SIXEL_BAD_ARGUMENT;
    *result = NULL;
    for (i = 0; i < sizeof(byte_dithers) / sizeof(*byte_dithers); ++i)
        if (byte_dithers[i].method == method)
            return byte_dithers[i].create(allocator, (void **)result);
    return SIXEL_FEATURE_ERROR;
}

SIXELSTATUS
sixel_gpu_dither_compat_prepare(sixel_allocator_t *allocator,
    sixel_dither_policy_prepare_request_t const *configuration,
    sixel_gpu_dither_prepare_request_t const *request,
    sixel_gpu_dither_plan_interface_t **result)
{
    sixel_gpu_dither_preparer_interface_t *preparer;
    SIXELSTATUS status;

    if (result == NULL || configuration == NULL) return SIXEL_BAD_ARGUMENT;
    *result = NULL;
    status = sixel_gpu_dither_byte_create(allocator,
        configuration->options.method_for_diffuse, &preparer);
    if (status != SIXEL_OK) return status;
    status = preparer->vtbl->configure(preparer, configuration);
    if (status == SIXEL_OK)
        status = preparer->vtbl->prepare(preparer, request, result);
    preparer->vtbl->unref(preparer);
    return status;
}
