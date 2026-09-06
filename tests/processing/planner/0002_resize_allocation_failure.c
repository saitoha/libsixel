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

static void *
praf_malloc(size_t size)
{
    if (size == PRAF_FLOAT_BYTES) {
        return NULL;
    }
    return malloc(size);
}

static void *
praf_calloc(size_t count, size_t size)
{
    if (size != 0u && count == PRAF_FLOAT_BYTES / size
        && count * size == PRAF_FLOAT_BYTES) {
        return NULL;
    }
    return calloc(count, size);
}

static void *
praf_realloc(void *ptr, size_t size)
{
    if (size == PRAF_FLOAT_BYTES) {
        return NULL;
    }
    return realloc(ptr, size);
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

    status = SIXEL_FALSE;
    message = NULL;
    sink_path = NULL;
    allocator = NULL;
    encoder = NULL;
    memset(pixels, 0x7f, sizeof(pixels));

    status = sixel_allocator_new(&allocator,
                                 praf_malloc,
                                 praf_calloc,
                                 praf_realloc,
                                 free);
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

    status = sixel_encoder_encode_bytes(encoder,
                                        pixels,
                                        PRAF_WIDTH,
                                        PRAF_HEIGHT,
                                        SIXEL_PIXELFORMAT_RGB888,
                                        NULL,
                                        0);
    if (status != SIXEL_BAD_ALLOCATION) {
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

    status = SIXEL_OK;

end:
    sixel_encoder_unref(encoder);
    sixel_allocator_unref(allocator);
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
