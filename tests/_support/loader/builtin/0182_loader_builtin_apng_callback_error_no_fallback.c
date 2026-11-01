/* Verify that an APNG callback error cannot invoke static fallback. */

#include "loader_builtin_memory_test_common.h"

static SIXELSTATUS
loader0182_reject_frame(sixel_frame_t *frame, void *data)
{
    unsigned int *callback_count;

    callback_count = (unsigned int *)data;
    if (frame == NULL || callback_count == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    ++*callback_count;
    return SIXEL_BAD_INPUT;
}

int
test_loader_0182_apng_callback_no_fallback(
    int argc,
    char **argv)
{
    edge_loader_options_t options;
    sixel_allocator_t *allocator;
    SIXELSTATUS status;
    unsigned int callback_count;
    int result;

    (void)argc;
    (void)argv;
    edge_loader_options_init(&options);
    allocator = NULL;
    status = SIXEL_FALSE;
    callback_count = 0u;
    status = sixel_allocator_new(&allocator, NULL, NULL, NULL, NULL);
    if (SIXEL_FAILED(status)) {
        return 1;
    }
    status = SIXEL_FALSE;
    result = edge_load_fixture_custom(
        "APNG callback failure",
        "/tests/data/inputs/formats/apng_8x8_blend_over.png",
        &options,
        allocator,
        loader0182_reject_frame,
        &callback_count,
        &status);
    sixel_allocator_unref(allocator);
    if (result != 0 || status != SIXEL_BAD_INPUT || callback_count != 1u) {
        return 1;
    }
    return 0;
}
