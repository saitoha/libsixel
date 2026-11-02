/* SPDX-License-Identifier: MIT */
#include <stdlib.h>
#include <string.h>
#include "src/lookup-policy-fhedt.h"

/* A prepared policy owns resolved settings even after the caller changes
 * its request. Each invocation selects one refinement mode. */
int
test_fhedt_lookup_configuration(int argc, char **argv)
{
    sixel_allocator_t *allocator;
    sixel_lookup_policy_interface_t *lookup;
    sixel_lookup_policy_prepare_request_t request;
    sixel_lookup_policy_configuration_t actual;
    unsigned char palette[6] = {0, 0, 0, 255, 255, 255};
    void *object;
    int refine, ok;

    if (argc != 2) return 1;
    refine = atoi(argv[1]);
    allocator = NULL;
    lookup = NULL;
    object = NULL;
    ok = 0;
    if (sixel_allocator_new(&allocator, NULL, NULL, NULL, NULL) != SIXEL_OK)
        goto end;
    if (sixel_lookup_policy_fhedt_8bit_new(allocator, &object) != SIXEL_OK)
        goto end;
    lookup = object;
    if (refine == 2) {
        ok = lookup->vtbl->get_configuration(lookup, &actual)
            == SIXEL_LOGIC_ERROR;
        goto end;
    }
    memset(&request, 0, sizeof(request));
    request.allocator = allocator;
    request.palette = palette;
    request.depth = 3;
    request.reqcolor = 2;
    request.pixelformat = SIXEL_PIXELFORMAT_RGB888;
    request.fhedt_resolution_override = 1;
    request.fhedt_resolution = 64;
    request.fhedt_refine_override = 1;
    request.fhedt_refine = refine;
    request.fhedt_use_dist2_override = 1;
    request.fhedt_use_dist2 = 1;
    request.fhedt_use_cache_override = 1;
    request.fhedt_use_cache = 0;
    if (lookup->vtbl->prepare(lookup, &request) != SIXEL_OK) goto end;
    memset(&request, 0, sizeof(request));
    if (lookup->vtbl->get_configuration(lookup, &actual) != SIXEL_OK)
        goto end;
    ok = actual.kind == SIXEL_LUT_POLICY_FHEDT &&
        actual.pixelformat == SIXEL_PIXELFORMAT_RGB888 &&
        actual.fhedt_resolution == 64 && actual.fhedt_refine == refine &&
        actual.fhedt_use_dist2 == 1 && actual.fhedt_use_cache == 0 &&
        actual.fhedt_weights[0] == 1 && actual.fhedt_weights[1] == 1 &&
        actual.fhedt_weights[2] == 1;
end:
    if (lookup != NULL) lookup->vtbl->unref(lookup);
    sixel_allocator_unref(allocator);
    return ok ? 0 : 1;
}
