/*
 * SPDX-License-Identifier: MIT
 *
 * Unit test for sixel_writer creation through the component factory.
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

typedef struct writer_capture {
    char bytes[256];
    int size;
    int calls;
} writer_capture_t;

static int
capture_write(char *data, int size, void *priv)
{
    writer_capture_t *capture;

    capture = (writer_capture_t *)priv;
    if (capture == NULL || data == NULL || size < 0 ||
        capture->size + size > (int)sizeof(capture->bytes)) {
        return -1;
    }

    memcpy(capture->bytes + capture->size, data, (size_t)size);
    capture->size += size;
    capture->calls += 1;

    return size;
}

static int
test_sixel_writer_case(sixel_factory_t *factory,
                       sixel_allocator_t *allocator,
                       int has_8bit_control,
                       char const *expected,
                       int expected_size)
{
    SIXELSTATUS status;
    sixel_writer_t *writer;
    sixel_writer_init_request_t request;
    sixel_writer_controls_t controls;
    sixel_writer_image_header_t header;
    writer_capture_t capture;
    void *object;

    status = SIXEL_FALSE;
    writer = NULL;
    object = NULL;
    memset(&request, 0, sizeof(request));
    memset(&controls, 0, sizeof(controls));
    memset(&header, 0, sizeof(header));
    memset(&capture, 0, sizeof(capture));

    status = factory->vtbl->create(factory, "io/sixel-writer",
                                   allocator, &object);
    if (SIXEL_FAILED(status)) {
        goto end;
    }

    writer = (sixel_writer_t *)object;
    object = NULL;
    if (writer == NULL || writer->vtbl == NULL ||
        writer->vtbl->init == NULL ||
        writer->vtbl->set_controls == NULL ||
        writer->vtbl->get_controls == NULL ||
        writer->vtbl->begin_image == NULL ||
        writer->vtbl->write == NULL ||
        writer->vtbl->end_image == NULL ||
        writer->vtbl->unref == NULL) {
        status = SIXEL_BAD_ARGUMENT;
        goto end;
    }

    request.fn_write = capture_write;
    request.priv = &capture;
    status = writer->vtbl->init(writer, &request);
    if (SIXEL_FAILED(status)) {
        goto end;
    }

    controls.has_8bit_control = has_8bit_control;
    status = writer->vtbl->set_controls(writer, &controls);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    memset(&controls, 0, sizeof(controls));
    status = writer->vtbl->get_controls(writer, &controls);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    if (controls.has_8bit_control != has_8bit_control) {
        status = SIXEL_BAD_ARGUMENT;
        goto end;
    }

    header.width = 2;
    header.height = 3;
    header.use_raster_attributes = 1;
    status = writer->vtbl->begin_image(writer, &header);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    status = writer->vtbl->write(writer, "abc", 3);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    status = writer->vtbl->end_image(writer);
    if (SIXEL_FAILED(status)) {
        goto end;
    }

    if (capture.size != expected_size ||
        memcmp(capture.bytes, expected, (size_t)expected_size) != 0 ||
        capture.calls < 3) {
        status = SIXEL_BAD_ARGUMENT;
        goto end;
    }

    status = SIXEL_OK;

end:
    if (object != NULL) {
        ((sixel_writer_t *)object)->vtbl->unref((sixel_writer_t *)object);
    }
    if (writer != NULL && writer->vtbl != NULL &&
        writer->vtbl->unref != NULL) {
        writer->vtbl->unref(writer);
    }
    return SIXEL_SUCCEEDED(status);
}

static int
test_sixel_writer_factory_component(void)
{
    SIXELSTATUS status;
    sixel_allocator_t *allocator;
    sixel_factory_t *factory;
    char const expected_7bit[] = "\033Pq\"1;1;2;3abc\033\\";
    char const expected_8bit[] = "\220q\"1;1;2;3abc\234";
    void *service;

    status = SIXEL_FALSE;
    allocator = NULL;
    factory = NULL;
    service = NULL;

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

    if (!test_sixel_writer_case(factory,
                                allocator,
                                0,
                                expected_7bit,
                                (int)sizeof(expected_7bit) - 1) ||
        !test_sixel_writer_case(factory,
                                allocator,
                                1,
                                expected_8bit,
                                (int)sizeof(expected_8bit) - 1)) {
        status = SIXEL_BAD_ARGUMENT;
        goto end;
    }

    status = SIXEL_OK;

end:
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
test_sixel_writer_0001_sixel_writer_factory(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    if (!test_sixel_writer_factory_component()) {
        fprintf(stderr, "sixel_writer factory component contract failed\n");
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
