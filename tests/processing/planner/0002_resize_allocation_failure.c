/*
 * SPDX-License-Identifier: MIT
 *
 * Verify that linear-light resize allocation failure stops encoding and
 * explains the explicit lower-memory policy and its accuracy trade-off.
 */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sixel.h>

#define PRAF_WIDTH 16
#define PRAF_HEIGHT 16
#define PRAF_FLOAT_BYTES \
    ((size_t)PRAF_WIDTH * PRAF_HEIGHT * 3u * sizeof(float))

/*
 * Public allocator callbacks have no userdata argument. The test runner
 * dispatches one test at a time, so this namespaced state can arm one exact
 * allocation without changing the allocator API or affecting setup.
 */
static size_t praf_failure_size;
static unsigned int praf_failure_count;
static unsigned int praf_live_allocation_count;

static int
praf_should_fail(size_t size)
{
    if (praf_failure_size != 0u && size == praf_failure_size) {
        praf_failure_size = 0u;
        ++praf_failure_count;
        return 1;
    }
    return 0;
}

static void *
praf_malloc(size_t size)
{
    void *ptr;

    ptr = NULL;
    if (praf_should_fail(size)) {
        return NULL;
    }
    ptr = malloc(size);
    if (ptr != NULL) {
        ++praf_live_allocation_count;
    }
    return ptr;
}

static void *
praf_calloc(size_t count, size_t size)
{
    size_t bytes;
    void *ptr;

    bytes = 0u;
    ptr = NULL;
    if (size != 0u && count <= (size_t)-1 / size) {
        bytes = count * size;
    }
    if (praf_should_fail(bytes)) {
        return NULL;
    }
    ptr = calloc(count, size);
    if (ptr != NULL) {
        ++praf_live_allocation_count;
    }
    return ptr;
}

static void *
praf_realloc(void *ptr, size_t size)
{
    void *resized;
    int had_allocation;

    resized = NULL;
    had_allocation = ptr != NULL;
    if (praf_should_fail(size)) {
        return NULL;
    }
    resized = realloc(ptr, size);
    if (!had_allocation && resized != NULL) {
        ++praf_live_allocation_count;
    }
    return resized;
}

static void
praf_free(void *ptr)
{
    if (ptr != NULL && praf_live_allocation_count != 0u) {
        --praf_live_allocation_count;
    }
    free(ptr);
}

static int
resize_allocation_failure_valid(void)
{
    SIXELSTATUS status;
    char const *message;
    char const *sink_path;
    sixel_allocator_t *allocator;
    sixel_encoder_t *encoder;
    unsigned char pixels[PRAF_WIDTH * PRAF_HEIGHT * 3];
    unsigned int live_allocation_baseline;

    status = SIXEL_FALSE;
    message = NULL;
    sink_path = NULL;
    allocator = NULL;
    encoder = NULL;
    live_allocation_baseline = 0u;
    praf_failure_size = 0u;
    praf_failure_count = 0u;
    praf_live_allocation_count = 0u;
    memset(pixels, 0x7f, sizeof(pixels));

    status = sixel_allocator_new(&allocator,
                                 praf_malloc,
                                 praf_calloc,
                                 praf_realloc,
                                 praf_free);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    status = sixel_encoder_new(&encoder, allocator);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    status = sixel_encoder_setopt(encoder,
                                  SIXEL_OPTFLAG_MONOCHROME,
                                  NULL);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    status = sixel_encoder_setopt(encoder,
                                  SIXEL_OPTFLAG_WIDTH,
                                  "50%");
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    status = sixel_encoder_setopt(encoder,
                                  SIXEL_OPTFLAG_RUNTIME_POLICY,
                                  "auto:Rlinear");
    if (SIXEL_FAILED(status)) {
        goto end;
    }

    /* Arm only the operation under test and consume exactly one failure. */
    live_allocation_baseline = praf_live_allocation_count;
    praf_failure_size = PRAF_FLOAT_BYTES;
    status = sixel_encoder_encode_bytes(encoder,
                                        pixels,
                                        PRAF_WIDTH,
                                        PRAF_HEIGHT,
                                        SIXEL_PIXELFORMAT_RGB888,
                                        NULL,
                                        0);
    praf_failure_size = 0u;
    if (status != SIXEL_BAD_ALLOCATION
        || praf_failure_count != 1u
        || praf_live_allocation_count != live_allocation_baseline) {
        status = SIXEL_LOGIC_ERROR;
        goto end;
    }

    message = sixel_helper_get_additional_message();
    if (message == NULL
        || strstr(message, "-j auto:resize_precision=preserve") == NULL
        || strstr(message, "reduce resampling accuracy") == NULL) {
        status = SIXEL_LOGIC_ERROR;
        goto end;
    }

#if defined(_WIN32)
    sink_path = "NUL";
#else
    sink_path = "/dev/null";
#endif
    status = sixel_encoder_setopt(encoder,
                                  SIXEL_OPTFLAG_OUTPUT,
                                  sink_path);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    status = sixel_encoder_setopt(encoder,
                                  SIXEL_OPTFLAG_RUNTIME_POLICY,
                                  "auto:Rpreserve");
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    status = sixel_encoder_encode_bytes(encoder,
                                        pixels,
                                        PRAF_WIDTH,
                                        PRAF_HEIGHT,
                                        SIXEL_PIXELFORMAT_RGB888,
                                        NULL,
                                        0);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    if (praf_failure_count != 1u) {
        status = SIXEL_LOGIC_ERROR;
        goto end;
    }

    status = SIXEL_OK;

end:
    praf_failure_size = 0u;
    sixel_encoder_unref(encoder);
    sixel_allocator_unref(allocator);
    if (praf_live_allocation_count != 0u) {
        status = SIXEL_LOGIC_ERROR;
    }
    return SIXEL_SUCCEEDED(status);
}

int
test_plan_0002_resize_alloc_failure(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    if (!resize_allocation_failure_valid()) {
        fprintf(stderr, "resize allocation failure contract failed\n");
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
