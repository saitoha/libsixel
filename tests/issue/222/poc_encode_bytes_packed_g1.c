/*
 * SPDX-License-Identifier: MIT
 *
 * Regression test for issue #222:
 * sixel_encoder_encode_bytes() must copy packed G1 input with packed byte
 * length, not width * height bytes.
 */

#include <stdio.h>
#include <stdlib.h>

#include <sixel.h>

static int g_capture_enabled;
static int g_capture_complete;
static size_t g_first_size;

static void
issue222_capture(size_t size)
{
    if (g_capture_enabled != 0 && g_capture_complete == 0) {
        g_capture_complete = 1;
        g_first_size = size;
    }
}

static void *
issue222_malloc(size_t size)
{
    void *result;

    result = malloc(size);
    issue222_capture(size);
    return result;
}

static void *
issue222_calloc(size_t nmemb, size_t size)
{
    void *result;

    result = calloc(nmemb, size);
    issue222_capture(nmemb * size);
    return result;
}

static void *
issue222_realloc(void *ptr, size_t size)
{
    void *result;

    result = realloc(ptr, size);
    issue222_capture(size);
    return result;
}

static void
issue222_free(void *ptr)
{
    free(ptr);
}

int
main(void)
{
    SIXELSTATUS status;
    sixel_allocator_t *allocator;
    sixel_encoder_t *encoder;
    unsigned char packed[3];
    char const *sink_path;
    int rc;

    status = SIXEL_FALSE;
    allocator = NULL;
    encoder = NULL;
    sink_path = NULL;
    rc = EXIT_FAILURE;

    packed[0] = 0xaau;
    packed[1] = 0x55u;
    packed[2] = 0x80u;

#if defined(_WIN32)
    sink_path = "NUL";
#else
    sink_path = "/dev/null";
#endif

    status = sixel_allocator_new(&allocator,
                                 issue222_malloc,
                                 issue222_calloc,
                                 issue222_realloc,
                                 issue222_free);
    if (SIXEL_FAILED(status)) {
        fprintf(stderr, "issue #222: sixel_allocator_new failed: %d\n", status);
        goto cleanup;
    }

    status = sixel_encoder_new(&encoder, allocator);
    if (SIXEL_FAILED(status)) {
        fprintf(stderr, "issue #222: sixel_encoder_new failed: %d\n", status);
        goto cleanup;
    }

    status = sixel_encoder_setopt(encoder, SIXEL_OPTFLAG_OUTPUT, sink_path);
    if (SIXEL_FAILED(status)) {
        fprintf(stderr, "issue #222: set output failed: %d\n", status);
        goto cleanup;
    }

    g_capture_enabled = 1;
    g_capture_complete = 0;
    g_first_size = 0u;
    status = sixel_encoder_encode_bytes(encoder,
                                        packed,
                                        17,
                                        1,
                                        SIXEL_PIXELFORMAT_G1,
                                        NULL,
                                        0);
    g_capture_enabled = 0;

    if (SIXEL_FAILED(status)) {
        fprintf(stderr,
                "issue #222: encode_bytes failed: %d (%s)\n",
                status,
                sixel_helper_get_additional_message());
        goto cleanup;
    }

    if (g_capture_complete == 0) {
        fprintf(stderr,
                "issue #222: source allocation capture did not trigger.\n");
        goto cleanup;
    }

    if (g_first_size != 3u) {
        fprintf(stderr,
                "issue #222: expected packed copy size 3, got %zu.\n",
                g_first_size);
        goto cleanup;
    }

    rc = EXIT_SUCCESS;

cleanup:
    if (encoder != NULL) {
        sixel_encoder_unref(encoder);
    }
    if (allocator != NULL) {
        sixel_allocator_unref(allocator);
    }

    return rc;
}

/* EOF */
