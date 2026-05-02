/*
 * SPDX-License-Identifier: MIT
 *
 * Unit test for output creation through the component factory.
 */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sixel.h>
#include <6cells.h>

#include "src/factory.h"
#include "src/output.h"

typedef struct output_capture {
    int calls;
    int bytes;
} output_capture_t;

static int
capture_write(char *data, int size, void *priv)
{
    output_capture_t *capture;

    capture = (output_capture_t *)priv;
    if (data == NULL || size < 0 || capture == NULL) {
        return -1;
    }

    capture->calls += 1;
    capture->bytes += size;
    return 0;
}

static int
test_output_factory_component(void)
{
    SIXELSTATUS status;
    sixel_allocator_t *allocator;
    sixel_factory_t *factory;
    sixel_output_interface_t *output;
    sixel_output_writer_request_t writer_request;
    sixel_output_options_t options;
    sixel_output_format_t format;
    output_capture_t capture;
    void *object;
    void *service;

    status = SIXEL_FALSE;
    allocator = NULL;
    factory = NULL;
    output = NULL;
    object = NULL;
    service = NULL;
    memset(&writer_request, 0, sizeof(writer_request));
    memset(&options, 0, sizeof(options));
    memset(&format, 0, sizeof(format));
    capture.calls = 0;
    capture.bytes = 0;

    status = sixel_allocator_new(&allocator, NULL, NULL, NULL, NULL);
    if (SIXEL_FAILED(status)) {
        goto end;
    }

    status = sixel_components_getservice("services/factory", &service);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    factory = (sixel_factory_t *)service;
    if (factory == NULL || factory->vtbl == NULL ||
        factory->vtbl->create == NULL ||
        factory->vtbl->unref == NULL) {
        status = SIXEL_BAD_ARGUMENT;
        goto end;
    }

    status = factory->vtbl->create(factory, "terminal/output",
                                   allocator, &object);
    if (SIXEL_FAILED(status)) {
        goto end;
    }

    output = (sixel_output_interface_t *)object;
    object = NULL;
    if (output == NULL || output->vtbl == NULL ||
        output->vtbl->init_writer == NULL ||
        output->vtbl->get_options == NULL ||
        output->vtbl->set_options == NULL ||
        output->vtbl->set_format == NULL ||
        output->vtbl->write == NULL ||
        output->vtbl->unref == NULL) {
        status = SIXEL_BAD_ARGUMENT;
        goto end;
    }

    writer_request.fn_write = capture_write;
    writer_request.priv = &capture;
    status = output->vtbl->init_writer(output, &writer_request);
    if (SIXEL_FAILED(status)) {
        goto end;
    }

    status = output->vtbl->get_options(output, &options);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    options.has_8bit_control = 1;
    options.skip_header = 1;
    options.encode_policy = SIXEL_ENCODEPOLICY_SIZE;
    status = output->vtbl->set_options(output, &options);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    memset(&options, 0, sizeof(options));
    status = output->vtbl->get_options(output, &options);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    if (options.has_8bit_control != 1 ||
        options.skip_header != 1 ||
        options.encode_policy != SIXEL_ENCODEPOLICY_SIZE) {
        status = SIXEL_BAD_ARGUMENT;
        goto end;
    }

    format.pixelformat = SIXEL_PIXELFORMAT_RGB888;
    format.source_colorspace = SIXEL_COLORSPACE_GAMMA;
    format.colorspace = SIXEL_COLORSPACE_LINEAR;
    status = output->vtbl->set_format(output, &format);
    if (SIXEL_FAILED(status)) {
        goto end;
    }

    status = output->vtbl->write(output, "abc", 3);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    if (capture.calls != 1 || capture.bytes != 3) {
        status = SIXEL_BAD_ARGUMENT;
        goto end;
    }

    status = SIXEL_OK;

end:
    if (object != NULL) {
        ((sixel_output_interface_t *)object)->vtbl->unref(
            (sixel_output_interface_t *)object);
    }
    if (output != NULL && output->vtbl != NULL &&
        output->vtbl->unref != NULL) {
        output->vtbl->unref(output);
    }
    if (factory != NULL && factory->vtbl != NULL &&
        factory->vtbl->unref != NULL) {
        factory->vtbl->unref(factory);
    }
    if (allocator != NULL) {
        sixel_allocator_unref(allocator);
    }
    return SIXEL_SUCCEEDED(status);
}

int
test_output_0001_output_factory(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    if (!test_output_factory_component()) {
        fprintf(stderr, "output factory component contract failed\n");
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
