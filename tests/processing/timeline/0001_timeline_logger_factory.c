/*
 * SPDX-License-Identifier: MIT
 *
 * Unit test for timeline writer service and logger factory creation.
 */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>

#include <sixel.h>
#include <6cells.h>

#include "src/compat_stub.h"
#include "src/factory.h"

/*
 * IDL usage in this unit
 *
 * IComponents.getservice("services/timeline-writer", &writer)
 * ITimelineWriter.create_logger(allocator, &logger)
 * ITimelineWriter.flush()
 * ITimelineLogger.log(event)
 * ITimelineLogger.set_frame_context(context)
 * ITimelineLogger.clear_frame_context()
 * ITimelineLogger.session_id()
 * ITimelineLogger.unref()
 * IFactory.create("diagnostics/timeline-logger", allocator, &object)
 */

static int
test_timeline_writer_creates_loggers(void)
{
    SIXELSTATUS status;
    sixel_allocator_t *allocator;
    sixel_factory_t *factory;
    sixel_timeline_writer_t *writer;
    sixel_timeline_logger_t *logger1;
    sixel_timeline_logger_t *logger2;
    sixel_timeline_logger_t *factory_logger;
    sixel_timeline_event_t event;
    sixel_timeline_frame_context_t frame_context;
    void *service;
    void *object;
    int logging_enabled;

    status = SIXEL_FALSE;
    allocator = NULL;
    factory = NULL;
    writer = NULL;
    logger1 = NULL;
    logger2 = NULL;
    factory_logger = NULL;
    service = NULL;
    object = NULL;
    logging_enabled = 0;

    status = sixel_allocator_new(&allocator, NULL, NULL, NULL, NULL);
    if (SIXEL_FAILED(status)) {
        goto end;
    }

    status = sixel_components_getservice("services/timeline-writer",
                                         &service);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    writer = (sixel_timeline_writer_t *)service;
    service = NULL;
    if (writer == NULL || writer->vtbl == NULL ||
        writer->vtbl->create_logger == NULL ||
        writer->vtbl->write == NULL ||
        writer->vtbl->flush == NULL ||
        writer->vtbl->unref == NULL) {
        status = SIXEL_BAD_ARGUMENT;
        goto end;
    }

    status = writer->vtbl->create_logger(writer, allocator, &logger1);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    status = writer->vtbl->create_logger(writer, allocator, &logger2);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    logging_enabled = logger1 != NULL || logger2 != NULL;
    if (!logging_enabled) {
        goto check_factory;
    }

    if (logger1 == NULL || logger2 == NULL ||
        logger1->vtbl == NULL || logger2->vtbl == NULL ||
        logger1->vtbl->log == NULL ||
        logger1->vtbl->set_frame_context == NULL ||
        logger1->vtbl->clear_frame_context == NULL ||
        logger1->vtbl->session_id == NULL ||
        logger1->vtbl->unref == NULL) {
        status = SIXEL_BAD_ARGUMENT;
        goto end;
    }
    if (logger1->vtbl->session_id(logger1) ==
        logger2->vtbl->session_id(logger2)) {
        status = SIXEL_BAD_ARGUMENT;
        goto end;
    }

    frame_context.frame_no = 3;
    frame_context.loop_no = 1;
    frame_context.multiframe = 1;
    status = logger1->vtbl->set_frame_context(logger1, &frame_context);
    if (SIXEL_FAILED(status)) {
        goto end;
    }

    event.role = "timeline/test";
    event.worker = "test";
    event.event = "start";
    event.job_id = 1;
    status = logger1->vtbl->log(logger1, &event);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    event.event = "finish";
    status = logger1->vtbl->log(logger1, &event);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    logger1->vtbl->clear_frame_context(logger1);
    writer->vtbl->flush(writer);

check_factory:
    status = sixel_components_getservice("services/factory", &service);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    factory = (sixel_factory_t *)service;
    service = NULL;
    if (factory == NULL || factory->vtbl == NULL ||
        factory->vtbl->create == NULL ||
        factory->vtbl->unref == NULL) {
        status = SIXEL_BAD_ARGUMENT;
        goto end;
    }
    status = factory->vtbl->create(factory,
                                   "diagnostics/timeline-logger",
                                   allocator,
                                   &object);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    factory_logger = (sixel_timeline_logger_t *)object;
    object = NULL;
    if (!logging_enabled) {
        if (factory_logger != NULL) {
            status = SIXEL_BAD_ARGUMENT;
            goto end;
        }
        status = SIXEL_OK;
        goto end;
    }
    if (factory_logger == NULL || factory_logger->vtbl == NULL ||
        factory_logger->vtbl->session_id == NULL ||
        factory_logger->vtbl->unref == NULL) {
        status = SIXEL_BAD_ARGUMENT;
        goto end;
    }

    status = SIXEL_OK;

end:
    if (object != NULL) {
        ((sixel_timeline_logger_t *)object)->vtbl->unref(
            (sixel_timeline_logger_t *)object);
    }
    if (factory_logger != NULL && factory_logger->vtbl != NULL &&
        factory_logger->vtbl->unref != NULL) {
        factory_logger->vtbl->unref(factory_logger);
    }
    if (logger2 != NULL && logger2->vtbl != NULL &&
        logger2->vtbl->unref != NULL) {
        logger2->vtbl->unref(logger2);
    }
    if (logger1 != NULL && logger1->vtbl != NULL &&
        logger1->vtbl->unref != NULL) {
        logger1->vtbl->unref(logger1);
    }
    if (factory != NULL && factory->vtbl != NULL &&
        factory->vtbl->unref != NULL) {
        factory->vtbl->unref(factory);
    }
    if (writer != NULL && writer->vtbl != NULL &&
        writer->vtbl->unref != NULL) {
        writer->vtbl->unref(writer);
    }
    if (allocator != NULL) {
        sixel_allocator_unref(allocator);
    }
    return SIXEL_SUCCEEDED(status);
}

int
test_timeline_0001_timeline_logger_factory(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    if (!test_timeline_writer_creates_loggers()) {
        fprintf(stderr, "timeline writer/logger component contract failed\n");
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
