/* SPDX-License-Identifier: MIT */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "src/encoder.h"
#include "src/options.h"
#include "src/presentation.h"
#include "src/sixel_decode_pixels.h"

#define LIMIT_REQUIRE(value) do { \
    if (!(value)) { \
        fprintf(stderr, "limits case %d, line %d\n", mode, __LINE__); \
        goto end; \
    } \
} while (0)

int
test_progressive_limits_options(int mode)
{
    sixel_encoder_t *encoder;
    char const *invalid[] = {
        "progressive:S-1", "progressive:Snan", "progressive:Sinf",
        "progressive:S1e155", "progressive:N-1", "progressive:N1.5",
        "progressive:N4294967296", "progressive:M-1", "progressive:Minf",
        "progressive:Mnan", "progressive:M1e999", "progressive:Sauto:N2:M?",
        "full:N1", "full:Sauto", "progressive:stop_threshold=garbage"
    };
    uint64_t revision;
    size_t i;
    int ok;

    encoder = NULL;
    ok = 0;
    LIMIT_REQUIRE(sixel_encoder_new(&encoder, NULL) == SIXEL_OK);
    LIMIT_REQUIRE(sixel_encoder_setopt(encoder, 'Z',
        "progressive:stop_threshold=0.01:max_passes=3:max_seconds=0.25")
        == SIXEL_OK);
    LIMIT_REQUIRE(encoder->progressive_stop_threshold == 0.01 &&
        encoder->progressive_max_passes == 3 &&
        encoder->progressive_max_seconds == 0.25);
    if (mode == 141) {
        revision = encoder->configuration_revision;
        for (i = 0; i < sizeof(invalid) / sizeof(invalid[0]); ++i) {
            LIMIT_REQUIRE(sixel_encoder_setopt(encoder, 'Z', invalid[i])
                == SIXEL_BAD_ARGUMENT);
            LIMIT_REQUIRE(encoder->progressive_stop_threshold == .01 &&
                encoder->progressive_max_passes == 3 &&
                encoder->progressive_max_seconds == .25 &&
                encoder->configuration_revision == revision &&
                encoder->update_policy == SIXEL_UPDATE_POLICY_PROGRESSIVE);
        }
    } else {
        LIMIT_REQUIRE(sixel_encoder_setopt(encoder, 'Z',
            "delta:P1:S0:N4294967295:M0") == SIXEL_OK);
        LIMIT_REQUIRE(encoder->progressive_enabled &&
            encoder->progressive_stop_threshold == 0 &&
            encoder->progressive_max_passes == UINT_MAX &&
            encoder->progressive_max_seconds == 0);
        LIMIT_REQUIRE(sixel_encoder_setopt(encoder, 'Z',
            "delta:P0:Sauto:N1:M0.1") == SIXEL_OK);
        LIMIT_REQUIRE(!encoder->progressive_enabled &&
            encoder->progressive_stop_threshold == SIXEL_OPTION_DOUBLE_AUTO);
        LIMIT_REQUIRE(sixel_encoder_setopt(encoder, 'Z', "progressive")
            == SIXEL_OK && encoder->progressive_enabled &&
            encoder->progressive_stop_threshold == SIXEL_OPTION_DOUBLE_AUTO
            && encoder->progressive_max_passes == 0 &&
            encoder->progressive_max_seconds ==
                SIXEL_PRESENTATION_DEFAULT_MAX_SECONDS);
    }
    ok = 1;
end:
    sixel_encoder_unref(encoder);
    return ok ? 0 : 1;
}

typedef struct limits_test_sink {
    unsigned char data[16384];
    size_t size;
    double time;
    double tick;
    double write_seconds;
    size_t clock_calls;
} limits_test_sink_t;

static double
limits_test_now(void *priv)
{
    limits_test_sink_t *sink;

    sink = priv;
    ++sink->clock_calls;
    sink->time += sink->tick;
    return sink->time;
}

static void
limits_test_wait(unsigned int usec, void *priv)
{
    limits_test_sink_t *sink;

    sink = priv;
    sink->time += usec / 1000000.0;
}

static int
limits_test_write(char *data, int size, void *priv)
{
    limits_test_sink_t *sink;

    sink = priv;
    if (size < 0 || (size_t)size > sizeof(sink->data) - sink->size) {
        return -1;
    }
    memcpy(sink->data + sink->size, data, (size_t)size);
    sink->size += (size_t)size;
    /* A deadline reached during an admitted DCS cannot truncate its tail. */
    if (size > 2) {
        sink->time += sink->write_seconds;
    }
    return 0;
}

int
test_progressive_limits_case(int mode)
{
    sixel_encoder_t *encoder;
    sixel_allocator_t *allocator;
    sixel_frame_t *source;
    sixel_output_t *output;
    sixel_presentation_t presentation;
    limits_test_sink_t sink;
    unsigned char black[192] = {0};
    char const baseline[] =
        "\0337\033P0;1q\"1;1;64;1#0;2;0;0;0#0!64@\033\\";
    float *pixels;
    unsigned char *decoded;
    unsigned int flags;
    size_t decoded_passes;
    uint64_t old_generation;
    double old_start;
    int width;
    int height;
    int i;
    int owned;
    int ok;
    SIXELSTATUS status;

    encoder = NULL;
    allocator = NULL;
    source = NULL;
    output = NULL;
    pixels = NULL;
    decoded = NULL;
    owned = ok = 0;
    memset(&presentation, 0, sizeof(presentation));
    memset(&sink, 0, sizeof(sink));
    memcpy(sink.data, baseline, sizeof(baseline) - 1);
    sink.size = sizeof(baseline) - 1;
    LIMIT_REQUIRE(sixel_allocator_new(&allocator, NULL, NULL, NULL, NULL)
        == SIXEL_OK);
    LIMIT_REQUIRE(sixel_encoder_new(&encoder, NULL) == SIXEL_OK);
    LIMIT_REQUIRE(sixel_encoder_setopt(encoder, 'Z',
        "progressive:S0:N3:M0") == SIXEL_OK);
    if (mode == 143 || mode == 144) {
        LIMIT_REQUIRE(sixel_encoder_setopt(encoder, 'Z',
            "progressive:S0:N0:M0.01") == SIXEL_OK);
    } else if (mode == 145) {
        LIMIT_REQUIRE(sixel_encoder_setopt(encoder, 'Z',
            "progressive:S2:N0:M0") == SIXEL_OK);
    } else if (mode == 146) {
        LIMIT_REQUIRE(sixel_encoder_setopt(encoder, 'Z',
            "progressive:S0:N2:M0.1") == SIXEL_OK);
    } else if (mode == 147 || mode == 148) {
        LIMIT_REQUIRE(sixel_encoder_setopt(encoder, 'Z', mode == 147
            ? "delta:P1:S0:N1:M0" : "delta:P1:S0:N0:M0.05") == SIXEL_OK);
        /* Simulate a build without async capacity, using the real clock
         * and final-tail policy without timing-dependent worker races. */
        LIMIT_REQUIRE(sixel_encoder_setopt(encoder, 'g', NULL) == SIXEL_OK);
    } else if (mode == 149) {
        LIMIT_REQUIRE(sixel_encoder_setopt(encoder, 'Z',
            "progressive:S0:N13:M0") == SIXEL_OK);
    }
    LIMIT_REQUIRE(sixel_encoder_setopt(encoder, SIXEL_OPTFLAG_GPU_POLICY,
        "off") == SIXEL_OK);
    LIMIT_REQUIRE(sixel_encoder_setopt(encoder, 'W', "gamma") == SIXEL_OK);
    LIMIT_REQUIRE(sixel_output_new(&output, limits_test_write, &sink, NULL)
        == SIXEL_OK);
    LIMIT_REQUIRE(sixel_encoder_operation_enter(encoder,
        SIXEL_ENCODER_OPERATION_NORMAL) == SIXEL_OK);
    owned = 1;
    LIMIT_REQUIRE(sixel_presentation_init_owned(&presentation, encoder,
        output, -1) == SIXEL_OK);
    encoder->fignore_delay = 0;
    presentation.now = limits_test_now;
    presentation.wait = limits_test_wait;
    presentation.clock_priv = &sink;
    presentation.saved = 1;
    presentation.repair->config.max_colors = 1;
    LIMIT_REQUIRE(presentation.repair->ladder.allowance == 2);
    sixel_encoder_operation_leave(encoder);
    owned = 0;
    LIMIT_REQUIRE(sixel_encoder_set_accumulation_buffer(encoder, black,
        64, 1, SIXEL_PIXELFORMAT_RGB888) == SIXEL_OK);
    pixels = malloc(sizeof(float) * 192);
    LIMIT_REQUIRE(pixels != NULL);
    for (i = 0; i < 192; ++i) {
        pixels[i] = (float)((37 * i + 51) % 256) / 255;
    }
    LIMIT_REQUIRE(sixel_frame_new(&source, NULL) == SIXEL_OK);
    LIMIT_REQUIRE(sixel_frame_init(source, (unsigned char *)pixels, 64, 1,
        SIXEL_PIXELFORMAT_RGBFLOAT32, NULL, 0) == SIXEL_OK);
    pixels = NULL;
    LIMIT_REQUIRE(sixel_encoder_operation_enter(encoder,
        SIXEL_ENCODER_OPERATION_NORMAL) == SIXEL_OK);
    owned = 1;
    LIMIT_REQUIRE(sixel_presentation_bind_owned(&presentation, source)
        == SIXEL_OK);
    sixel_encoder_operation_leave(encoder);
    owned = 0;
    if (mode == 142) {
        presentation.budget.max_pixel_visits = 1;
        presentation.budget.max_distance_evaluations = 1;
    } else if (mode == 143) {
        sink.tick = .001;
    } else if (mode == 144) {
        sink.write_seconds = .02;
    }
    if (mode == 147 || mode == 148) {
        LIMIT_REQUIRE(sixel_presentation_before_frame(&presentation)
            == SIXEL_OK);
        LIMIT_REQUIRE(sixel_presentation_after_frame(&presentation, 10, 1)
            == SIXEL_OK);
        LIMIT_REQUIRE(fabs(sink.time - .1) < .000002 &&
            fabs(presentation.deadline - .1) < .000002);
    }
    LIMIT_REQUIRE(sixel_presentation_finish(&presentation) == SIXEL_OK);
    if (mode == 143 || mode == 148) {
        LIMIT_REQUIRE(presentation.stop_reason ==
            SIXEL_PRESENTATION_STOP_SECONDS && presentation.passes == 1 &&
            sink.size == sizeof(baseline) - 1);
    } else if (mode == 144) {
        LIMIT_REQUIRE(presentation.stop_reason ==
            SIXEL_PRESENTATION_STOP_SECONDS && presentation.passes == 2 &&
            sink.time >= .02);
    } else if (mode == 145) {
        LIMIT_REQUIRE(presentation.stop_reason ==
            SIXEL_PRESENTATION_STOP_CONVERGED && presentation.passes == 1);
    } else {
        LIMIT_REQUIRE(presentation.stop_reason ==
            SIXEL_PRESENTATION_STOP_PASSES &&
            presentation.passes == presentation.max_passes);
    }
    if (mode == 142) {
        LIMIT_REQUIRE(sink.clock_calls > 64);
    } else if (mode == 149) {
        LIMIT_REQUIRE(presentation.repair->ladder.count == 5 &&
            presentation.repair->ladder.rung == 4 &&
            presentation.repair->ladder.noise_off &&
            presentation.repair->zero_noise);
    }
    LIMIT_REQUIRE(sixel_decode_sequence_mapped(sink.data, sink.size,
        NULL, &decoded, &width, &height, &flags, &decoded_passes, allocator)
        == SIXEL_OK && width == 64 && height == 1 &&
        decoded_passes == presentation.passes && encoder->accumulation_valid);
    for (i = 0; i < 64; ++i) {
        LIMIT_REQUIRE(memcmp(decoded + i * 4,
            encoder->accumulation_pixels + i * 3, 3) == 0);
    }
    if (mode == 146) {
        old_start = presentation.source_started;
        old_generation = presentation.generation;
        sink.time = 1;
        LIMIT_REQUIRE(sixel_encoder_operation_enter(encoder,
            SIXEL_ENCODER_OPERATION_NORMAL) == SIXEL_OK);
        owned = 1;
        presentation.generation = UINT64_MAX;
        LIMIT_REQUIRE(sixel_presentation_bind_owned(&presentation, source)
            == SIXEL_BAD_INTEGER_OVERFLOW && presentation.passes == 2 &&
            presentation.source_started == old_start);
        presentation.generation = old_generation;
        LIMIT_REQUIRE(sixel_presentation_bind_owned(&presentation, source)
            == SIXEL_OK && presentation.source_started == 1 &&
            presentation.passes == 1 && !presentation.finished &&
            presentation.stop_reason == SIXEL_PRESENTATION_STOP_NONE &&
            presentation.repair->ladder.count == 0);
        sixel_encoder_operation_leave(encoder);
        owned = 0;
        LIMIT_REQUIRE(sixel_presentation_finish(&presentation) == SIXEL_OK
            && presentation.passes == 2 && presentation.stop_reason ==
                SIXEL_PRESENTATION_STOP_PASSES);
    }
    ok = 1;
end:
    if (owned) {
        sixel_encoder_operation_leave(encoder);
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
    free(pixels);
    sixel_allocator_free(allocator, decoded);
    sixel_frame_unref(source);
    sixel_output_unref(output);
    sixel_encoder_unref(encoder);
    sixel_allocator_unref(allocator);
    return ok ? 0 : 1;
}
