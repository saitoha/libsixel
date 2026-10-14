/* SPDX-License-Identifier: MIT */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "src/encoder.h"
#include "src/presentation.h"
#include "src/output.h"

int test_progressive_public_case(int mode);

#define SCHEDULE_REQUIRE(value) do { \
    if (!(value)) { \
        fprintf(stderr, "schedule case %d, line %d\n", mode, __LINE__); \
        goto end; \
    } \
} while (0)

typedef struct presentation_test_clock {
    double time;
    int waits;
    int *cancel;
} presentation_test_clock_t;

static double
presentation_test_now(void *priv)
{
    return ((presentation_test_clock_t *)priv)->time;
}

static void
presentation_test_wait(unsigned int usec, void *priv)
{
    presentation_test_clock_t *clock;

    clock = priv;
    clock->time += usec / 1000000.0;
    ++clock->waits;
    if (clock->cancel != NULL) {
        *clock->cancel = 1;
    }
}

int
test_progressive_schedule_case(int mode)
{
    sixel_encoder_t *encoder;
    sixel_presentation_t presentation;
    sixel_repair_context_t repair;
    presentation_test_clock_t clock;
    size_t bytes;
    double deadline;
    int cancel;
    int ok;

    encoder = NULL;
    memset(&presentation, 0, sizeof(presentation));
    memset(&repair, 0, sizeof(repair));
    memset(&clock, 0, sizeof(clock));
    cancel = ok = 0;
    bytes = 99;
    deadline = -1;
    if (mode == 109) {
        SCHEDULE_REQUIRE(!sixel_presentation_admission(1, 2, 0.002,
            0.0001, 1, 1024, &bytes, &deadline));
        SCHEDULE_REQUIRE(bytes == 99 && deadline == -1);
        SCHEDULE_REQUIRE(sixel_presentation_admission(1, 2, 0.002,
            0.0001, 0, 1024, &bytes, &deadline));
        SCHEDULE_REQUIRE(bytes == 1022 && deadline > 1 && deadline < 2);
        /* The cursor prefix is charged to the same reservation. */
        SCHEDULE_REQUIRE(sixel_presentation_admission(1, 1.03, 0.002,
            0.0001, 0, 1024, &bytes, &deadline));
        SCHEDULE_REQUIRE(bytes <= 128 && deadline > 1);
        ok = 1;
        goto end;
    }
    if (mode == 110) {
        SCHEDULE_REQUIRE(!sixel_presentation_admission(2, 2, 0.002,
            0.0001, 0, 16384, &bytes, &deadline));
        SCHEDULE_REQUIRE(!sixel_presentation_admission(1, 2, 0.002,
            0, 0, 16384, &bytes, &deadline));
        SCHEDULE_REQUIRE(!sixel_presentation_admission(1, 2, 0.002,
            0.0001, 0, 65, &bytes, &deadline));
        SCHEDULE_REQUIRE(!sixel_presentation_admission(NAN, 2, 0.002,
            0.0001, 0, 16384, &bytes, &deadline));
        SCHEDULE_REQUIRE(test_progressive_public_case(110) == 0);
        ok = 1;
        goto end;
    }
    SCHEDULE_REQUIRE(sixel_encoder_new(&encoder, NULL) == SIXEL_OK);
    SCHEDULE_REQUIRE(sixel_encoder_setopt(encoder, 'Z', "delta:P1")
        == SIXEL_OK);
    presentation.encoder = encoder;
    presentation.repair = &repair;
    presentation.finished = 1;
    presentation.now = presentation_test_now;
    presentation.wait = presentation_test_wait;
    presentation.clock_priv = &clock;
    SCHEDULE_REQUIRE(sixel_presentation_before_frame(&presentation)
        == SIXEL_OK);
    if (mode == 111) {
        clock.time = 0.03; /* The first ordinary encoding consumed 30 ms. */
        SCHEDULE_REQUIRE(sixel_presentation_after_frame(&presentation, 10, 1)
            == SIXEL_OK && fabs(clock.time - 0.1) < 0.000002);
        clock.time += 0.025;
        SCHEDULE_REQUIRE(sixel_presentation_after_frame(&presentation, 10, 1)
            == SIXEL_OK && fabs(clock.time - 0.2) < 0.000002);
        clock.time = 0.5; /* Missed frames never restart the source clock. */
        SCHEDULE_REQUIRE(sixel_presentation_after_frame(&presentation, 10, 1)
            == SIXEL_OK && clock.time == 0.5 &&
            fabs(presentation.deadline - 0.3) < 0.000001);
    } else if (mode == 112) {
        SCHEDULE_REQUIRE(sixel_presentation_after_frame(&presentation, 0, 1)
            == SIXEL_OK && clock.waits == 0);
        SCHEDULE_REQUIRE(sixel_encoder_setopt(encoder, 'g', NULL) == SIXEL_OK);
        SCHEDULE_REQUIRE(sixel_presentation_after_frame(&presentation, 10, 1)
            == SIXEL_OK && clock.waits == 0 && clock.time == 0);
    } else if (mode == 113) {
        encoder->cancel_flag = &cancel;
        clock.cancel = &cancel;
        SCHEDULE_REQUIRE(sixel_presentation_after_frame(&presentation, 10, 1)
            == SIXEL_INTERRUPTED && clock.waits == 1 && clock.time < 0.1);
    }
    ok = 1;
end:
    sixel_encoder_unref(encoder);
    return ok ? 0 : 1;
}

#if SIXEL_ENABLE_THREADS
typedef struct presentation_test_sink {
    sixel_mutex_t mutex;
    sixel_cond_t cond;
    unsigned char data[128];
    size_t bytes;
    int entered;
    int released;
    int failed;
} presentation_test_sink_t;

static int
presentation_test_sink_write(char *data, int size, void *priv)
{
    presentation_test_sink_t *sink;
    int result;

    sink = priv;
    sixel_mutex_lock(&sink->mutex);
    sink->entered = 1;
    sixel_cond_broadcast(&sink->cond);
    while (!sink->released) {
        sixel_cond_wait(&sink->cond, &sink->mutex);
    }
    result = sink->failed ? -1 : 0;
    if (result == 0 && sink->bytes + size <= sizeof(sink->data)) {
        memcpy(sink->data + sink->bytes, data, size);
        sink->bytes += size;
    } else {
        result = -1;
    }
    sixel_mutex_unlock(&sink->mutex);
    return result;
}
#endif

int
test_progressive_transport_case(int mode)
{
#if SIXEL_ENABLE_THREADS
    sixel_encoder_t *encoder;
    sixel_presentation_t presentation;
    sixel_output_t *output;
    presentation_test_sink_t sink;
    SIXELSTATUS status;
    size_t pending;
    int initialized;
    int owned;
    int ok;

    encoder = NULL;
    output = NULL;
    memset(&presentation, 0, sizeof(presentation));
    memset(&sink, 0, sizeof(sink));
    initialized = owned = ok = 0;
    SCHEDULE_REQUIRE(sixel_mutex_init(&sink.mutex) == 0);
    SCHEDULE_REQUIRE(sixel_cond_init(&sink.cond) == 0);
    initialized = 1;
    SCHEDULE_REQUIRE(sixel_encoder_new(&encoder, NULL) == SIXEL_OK);
    SCHEDULE_REQUIRE(sixel_encoder_setopt(encoder, 'Z', "delta:P1")
        == SIXEL_OK);
    SCHEDULE_REQUIRE(sixel_output_new(&output, presentation_test_sink_write,
        &sink, NULL) == SIXEL_OK);
    SCHEDULE_REQUIRE(sixel_encoder_operation_enter(encoder,
        SIXEL_ENCODER_OPERATION_NORMAL) == SIXEL_OK);
    owned = 1;
    SCHEDULE_REQUIRE(sixel_presentation_init_owned(&presentation, encoder,
        output, -1) == SIXEL_OK && presentation.transport_started);
    sixel_encoder_operation_leave(encoder);
    owned = 0;
    SCHEDULE_REQUIRE(sixel_presentation_begin_image(&presentation, 8)
        == SIXEL_OK);
    presentation.async_submission = 1;
    /* Queue acceptance must complete while the physical sink is blocked. */
    SCHEDULE_REQUIRE(sixel_output_write_bytes(presentation.output, "first", 5)
        == SIXEL_OK);
    sixel_mutex_lock(&sink.mutex);
    while (!sink.entered) {
        sixel_cond_wait(&sink.cond, &sink.mutex);
    }
    sixel_mutex_unlock(&sink.mutex);
    sixel_mutex_lock(&presentation.transport_mutex);
    pending = presentation.transport_bytes;
    sixel_mutex_unlock(&presentation.transport_mutex);
    SCHEDULE_REQUIRE(pending == 7);
    sixel_mutex_lock(&sink.mutex);
    sink.failed = mode == 115;
    sink.released = 1;
    sixel_cond_broadcast(&sink.cond);
    sixel_mutex_unlock(&sink.mutex);
    status = sixel_presentation_before_frame(&presentation);
    if (mode == 115) {
        SCHEDULE_REQUIRE(status != SIXEL_OK && sink.bytes == 0);
        SCHEDULE_REQUIRE(sixel_presentation_shutdown(&presentation)
            != SIXEL_OK);
    } else {
        SCHEDULE_REQUIRE(status == SIXEL_OK);
        presentation.async_submission = 0;
        SCHEDULE_REQUIRE(sixel_presentation_begin_image(&presentation, 8)
            == SIXEL_OK);
        SCHEDULE_REQUIRE(sixel_output_write_bytes(presentation.output,
            "second", 6) == SIXEL_OK);
        SCHEDULE_REQUIRE(sink.bytes == 15 &&
            memcmp(sink.data, "\0337first\0338second", 15) == 0);
    }
    ok = 1;
end:
    if (owned) {
        sixel_encoder_operation_leave(encoder);
    }
    if (initialized) {
        sixel_mutex_lock(&sink.mutex);
        sink.released = 1;
        sixel_cond_broadcast(&sink.cond);
        sixel_mutex_unlock(&sink.mutex);
    }
    (void)sixel_presentation_shutdown(&presentation);
    if (encoder != NULL) {
        status = sixel_encoder_operation_enter(encoder,
            SIXEL_ENCODER_OPERATION_NORMAL);
        if (status == SIXEL_OK) {
            sixel_presentation_dispose_owned(&presentation);
            sixel_encoder_operation_leave(encoder);
        }
    }
    sixel_output_unref(output);
    sixel_encoder_unref(encoder);
    if (initialized) {
        sixel_cond_destroy(&sink.cond);
        sixel_mutex_destroy(&sink.mutex);
    }
    return ok ? 0 : 1;
#else
    (void)mode;
    return 77;
#endif
}
