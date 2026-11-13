/* SPDX-License-Identifier: MIT */
#include "src/lookup-policy-ordered.h"

int
test_ordered_float_policy_identity(int argc, char **argv)
{
    sixel_allocator_t *allocator;
    sixel_lookup_policy_interface_t *policy;
    void *object;
    SIXELSTATUS status;
    int result;

    (void)argc;
    (void)argv;
    allocator = NULL;
    object = NULL;
    result = 1;
    status = sixel_allocator_new(&allocator, NULL, NULL, NULL, NULL);
    if (status != SIXEL_OK) return 1;
    status = sixel_lookup_policy_ordered_float32_new(allocator, &object);
    if (status == SIXEL_OK) {
        policy = object;
        result = policy->vtbl->get_kind(policy) != SIXEL_LUT_POLICY_ORDERED;
        policy->vtbl->unref(policy);
    }
    sixel_allocator_unref(allocator);
    return result;
}
