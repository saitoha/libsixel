/* SPDX-License-Identifier: MIT */
/* Every failed capture allocation leaves no image or retained allocations. */
#include <stdlib.h>
#include <string.h>
#include <sixel.h>
#include "../../../src/decoder.h"

static size_t sequence_alloc_live;
static size_t sequence_alloc_call;
static size_t sequence_alloc_fail;

static void *sequence_test_malloc(size_t size)
{
    void *p;

    if (++sequence_alloc_call == sequence_alloc_fail) {
        return NULL;
    }
    p = malloc(size);
    if (p != NULL) {
        ++sequence_alloc_live;
    }
    return p;
}

static void *sequence_test_calloc(size_t count, size_t size)
{
    void *p;

    p = sequence_test_malloc(count * size);
    if (p != NULL) {
        memset(p, 0, count * size);
    }
    return p;
}

static void *sequence_test_realloc(void *old, size_t size)
{
    void *p;
    int was_null;

    if (++sequence_alloc_call == sequence_alloc_fail) {
        return NULL;
    }
    was_null = old == NULL;
    p = realloc(old, size);
    if (p != NULL && was_null) {
        ++sequence_alloc_live;
    }
    return p;
}

static void sequence_test_free(void *p)
{
    if (p != NULL) {
        --sequence_alloc_live;
    }
    free(p);
}

int test_sequence_0086(int argc, char **argv)
{
    static unsigned char const stream[] =
        "\033P0;1q\"1;1;1;1#0;2;100;0;0@\033\\"
        "\033P0;1q\"1;1;1;1#0;2;0;0;100@\033\\";
    sixel_allocator_t *allocator;
    sixel_decoder_t *decoder;
    sixel_decode_result_t result;
    SIXELSTATUS status;
    size_t baseline;
    size_t fail;
    int ok;
    int completed;

    (void)argc;
    (void)argv;
    allocator = NULL;
    decoder = NULL;
    sequence_alloc_live = 0U;
    sequence_alloc_call = 0U;
    sequence_alloc_fail = 0U;
    status = sixel_allocator_new(&allocator, sequence_test_malloc,
        sequence_test_calloc, sequence_test_realloc, sequence_test_free);
    if (SIXEL_FAILED(status)) {
        return EXIT_FAILURE;
    }
    status = sixel_decoder_new(&decoder, allocator);
    if (SIXEL_FAILED(status)) {
        sixel_allocator_unref(allocator);
        return EXIT_FAILURE;
    }
    decoder->dequantize_method = SIXEL_DEQUANTIZE_NONE;
    decoder->gpu_policy = SIXEL_GPU_POLICY_OFF;
    decoder->progressive_input = 1;
    baseline = sequence_alloc_live;
    ok = 1;
    completed = 0;
    for (fail = 1U; fail <= 32U; ++fail) {
        memset(&result, 0, sizeof(result));
        sequence_alloc_call = 0U;
        sequence_alloc_fail = fail;
        status = sixel_decoder_decode_pixels(decoder, stream,
            sizeof(stream) - 1U, NULL, &result);
        if (SIXEL_SUCCEEDED(status)) {
            completed = result.pixels != NULL &&
                result.pixels[2] == 255 && decoder->input_passes == 2U;
        } else if (result.pixels != NULL) {
            ok = 0;
        }
        sixel_allocator_free(allocator, result.pixels);
        ok = ok && sequence_alloc_live == baseline;
        if (completed || !ok) {
            break;
        }
    }
    sequence_alloc_fail = 0U;
    sixel_decoder_unref(decoder);
    sixel_allocator_unref(allocator);
    return ok && completed && sequence_alloc_live == 0U ?
        EXIT_SUCCESS : EXIT_FAILURE;
}
