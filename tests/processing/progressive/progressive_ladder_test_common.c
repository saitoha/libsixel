/* SPDX-License-Identifier: MIT */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sixel.h>
#include "src/encoder.h"
#include "src/progressive.h"
#include "src/progressive-measure.h"

typedef struct ladder_test_sink {
    unsigned char data[16384];
    size_t size;
    int fail;
} ladder_test_sink_t;

static int
ladder_test_write(char *data, int size, void *priv)
{
    ladder_test_sink_t *sink;

    sink = priv;
    if (sink->fail || size < 0 ||
            (size_t)size > sizeof(sink->data) - sink->size) {
        return -1;
    }
    memcpy(sink->data + sink->size, data, (size_t)size);
    sink->size += (size_t)size;
    return 0;
}

#define LADDER_REQUIRE(condition) do { \
    if (!(condition)) { \
        fprintf(stderr, "ladder assertion at line %d\n", __LINE__); \
        goto end; \
    } \
} while (0)

int
test_progressive_ladder_knots(int mode)
{
    sixel_repair_ladder_t ladder;
    double errors[16];
    double expected[5] = {15, 13, 9, 5, 0};
    size_t order[16];
    unsigned int allowance;
    unsigned int rung;
    unsigned int i;
    unsigned int j;
    int ok;

    ok = 0;
    memset(&ladder, 0, sizeof(ladder));
    for (i = 0; i < 16; ++i) {
        errors[i] = 16 - i;
        order[i] = i;
    }
    if (mode == 131) {
        /* Boundary ties stay together; the minimum knot already selects
         * the full domain and therefore collapses into the exact floor. */
        for (i = 0; i < 16; ++i) {
            errors[i] = i < 8 ? 2 : 1;
        }
        sixel_repair_ladder_build(&ladder, errors, order, 16, 0);
        LADDER_REQUIRE(ladder.count == 2 && ladder.thresholds[0] == 2 &&
            ladder.thresholds[1] == 0 && !ladder.noise_off);
        memset(&ladder, 0, sizeof(ladder));
        sixel_repair_ladder_build(&ladder, errors, order, 8, .5);
        LADDER_REQUIRE(ladder.count == 1 && ladder.thresholds[0] == .5 &&
            ladder.noise_off);
        memset(&ladder, 0, sizeof(ladder));
        sixel_repair_ladder_build(&ladder, errors, order, 0, 0);
        LADDER_REQUIRE(ladder.count == 1 && ladder.thresholds[0] == 0 &&
            ladder.noise_off);
    } else {
        for (allowance = 1; allowance <= 4; allowance *= 2) {
            memset(&ladder, 0, sizeof(ladder));
            ladder.allowance = allowance;
            sixel_repair_ladder_build(&ladder, errors, order, 16, 0);
            LADDER_REQUIRE(ladder.count == 5 && !ladder.noise_off &&
                memcmp(ladder.thresholds, expected, sizeof(expected)) == 0);
            /* A later distribution cannot recompute this binding's knots. */
            sixel_repair_ladder_build(&ladder, errors, order, 1, .25);
            LADDER_REQUIRE(memcmp(ladder.thresholds, expected,
                sizeof(expected)) == 0);
            if (mode == 132) {
                for (rung = 0; rung < 4; ++rung) {
                    for (j = 0; j < allowance; ++j) {
                        LADDER_REQUIRE(ladder.rung == rung &&
                            ladder.emissions == j && !ladder.noise_off);
                        sixel_repair_ladder_commit(&ladder);
                    }
                }
                LADDER_REQUIRE(ladder.rung == 4 && ladder.emissions == 0 &&
                    ladder.noise_off);
                sixel_repair_ladder_commit(&ladder);
                LADDER_REQUIRE(!sixel_repair_ladder_descend(&ladder,
                    SIXEL_REPAIR_LADDER_NO_PAINT) &&
                    ladder.rung == 4 && ladder.emissions == 0 &&
                    ladder.noise_off);
            }
        }
    }
    ok = 1;
end:
    return ok ? 0 : 1;
}

static SIXELSTATUS
ladder_test_step(sixel_progressive_t *context, size_t quantum,
    size_t bytes, sixel_output_t *output, sixel_repair_result_t *result,
    size_t *calls)
{
    sixel_repair_work_t work;
    SIXELSTATUS status;

    for (*calls = 0; *calls < 100000; ++*calls) {
        memset(&work, 0, sizeof(work));
        work.pixel_limit = work.distance_limit = quantum;
        status = sixel_repair_context_step(context, &work, bytes, output,
            result);
        if (status != SIXEL_OK ||
                result->audit.outcome != SIXEL_REPAIR_YIELDED) {
            return status;
        }
    }
    return SIXEL_LOGIC_ERROR;
}

int
test_progressive_ladder_case(int mode)
{
    sixel_encoder_t *encoder;
    sixel_progressive_t *context;
    sixel_progressive_config_t config;
    sixel_repair_result_t result;
    sixel_repair_measure_record_t record;
    sixel_repair_ladder_t saved_ladder;
    sixel_frame_t *source;
    sixel_output_t *output;
    ladder_test_sink_t sink;
    unsigned char black[192] = {0};
    float target[192];
    unsigned char saved[16384];
    unsigned int emissions[5] = {0};
    unsigned char *owned;
    double thresholds[5];
    SIXELSTATUS status;
    size_t quantum;
    size_t calls;
    size_t saved_size;
    size_t pass;
    size_t count;
    unsigned int rung;
    unsigned int last_rung;
    unsigned int final_packets;
    int run;
    int i;
    int ok;

    encoder = NULL;
    context = NULL;
    source = NULL;
    output = NULL;
    owned = NULL;
    saved_size = 0;
    ok = 0;
    count = mode == 136 || mode == 138 ? 2 : 64;
    for (i = 0; i < 192; ++i) {
        target[i] = (float)((37 * i + 51) % 256) / 255;
    }
    if (mode == 136) {
        /* The worst outlier has no improving projected output; smaller
         * gray residuals must still be reached even with a one-color cap. */
        target[0] = 2;
        target[1] = target[2] = 0;
        target[3] = target[4] = target[5] = .5f;
        black[0] = 255;
    } else if (mode == 138) {
        /* The source is between RGB101 codes but the accumulator already
         * holds its directly projected code. This is not convergence. */
        for (i = 0; i < 6; ++i) {
            target[i] = .351f;
            black[i] = 89;
        }
    }
    for (run = 0; run < (mode == 133 ? 2 : 1); ++run) {
        quantum = mode == 133 && run == 1 ? 1 : 1048576;
        memset(&sink, 0, sizeof(sink));
        memset(emissions, 0, sizeof(emissions));
        final_packets = last_rung = 0;
        LADDER_REQUIRE(sixel_encoder_new(&encoder, NULL) == SIXEL_OK);
        LADDER_REQUIRE(sixel_encoder_setopt(encoder,
            SIXEL_OPTFLAG_UPDATE_POLICY, "delta") == SIXEL_OK);
        LADDER_REQUIRE(sixel_encoder_setopt(encoder,
            SIXEL_OPTFLAG_GPU_POLICY, "off") == SIXEL_OK);
        LADDER_REQUIRE(sixel_encoder_setopt(encoder,
            SIXEL_OPTFLAG_WORKING_COLORSPACE, "gamma") == SIXEL_OK);
        LADDER_REQUIRE(sixel_encoder_set_accumulation_buffer(encoder,
            black, (int)count, 1, SIXEL_PIXELFORMAT_RGB888) == SIXEL_OK);
        LADDER_REQUIRE(sixel_progressive_config_init(&config,
            sizeof(config)) == SIXEL_OK);
        config.stop_distance_auto = 0;
        config.max_colors = 1;
        config.sample_limit = mode == 136 ? 1 : 64;
        LADDER_REQUIRE(sixel_progressive_new(&context, encoder, &config,
            NULL) == SIXEL_OK);
        LADDER_REQUIRE(sixel_repair_ladder_enable(context, 3)
            == SIXEL_BAD_ARGUMENT);
        LADDER_REQUIRE(sixel_repair_ladder_enable(context, 2) == SIXEL_OK);
        LADDER_REQUIRE(sixel_repair_measure_enable(context) == SIXEL_OK);
        owned = malloc(count * 3 * sizeof(float));
        LADDER_REQUIRE(owned != NULL);
        memcpy(owned, target, count * 3 * sizeof(float));
        LADDER_REQUIRE(sixel_frame_new(&source, NULL) == SIXEL_OK);
        LADDER_REQUIRE(sixel_frame_init(source, owned, (int)count, 1,
            SIXEL_PIXELFORMAT_RGBFLOAT32, NULL, 0) == SIXEL_OK);
        owned = NULL;
        LADDER_REQUIRE(sixel_progressive_bind_frame(context, source,
            0, 0, 1) == SIXEL_OK);
        LADDER_REQUIRE(sixel_repair_ladder_enable(context, 1)
            == SIXEL_BAD_ARGUMENT);
        LADDER_REQUIRE(sixel_output_new(&output, ladder_test_write,
            &sink, NULL) == SIXEL_OK);
        if (mode == 134 || mode == 135) {
            /* A completed packet that cannot be sent spends no allowance;
             * expanding capacity resumes the same decision. */
            status = ladder_test_step(context, 1048576, 1, output, &result,
                &calls);
            LADDER_REQUIRE(status == SIXEL_OK &&
                result.audit.outcome == SIXEL_REPAIR_BUDGET_TOO_SMALL &&
                context->ladder.count == 5 && context->ladder.rung == 0 &&
                context->ladder.emissions == 0 && sink.size == 0);
            saved_ladder = context->ladder;
            status = ladder_test_step(context, 1, 1, output, &result, &calls);
            LADDER_REQUIRE(status == SIXEL_OK &&
                result.audit.outcome == SIXEL_REPAIR_BUDGET_TOO_SMALL &&
                memcmp(&saved_ladder, &context->ladder,
                    sizeof(saved_ladder)) == 0);
            sink.fail = mode == 135;
            status = ladder_test_step(context, 1048576, 16384, output,
                &result, &calls);
            if (mode == 135) {
                LADDER_REQUIRE(status != SIXEL_OK && sink.size == 0 &&
                    memcmp(&saved_ladder, &context->ladder,
                        sizeof(saved_ladder)) == 0);
                LADDER_REQUIRE(ladder_test_step(context, 1048576, 16384,
                    output, &result, &calls) == SIXEL_OK &&
                    result.audit.outcome == SIXEL_REPAIR_NEEDS_BASELINE);
            } else {
                LADDER_REQUIRE(status == SIXEL_OK && sink.size > 0 &&
                    result.audit.outcome == SIXEL_REPAIR_FRAME_EMITTED &&
                    context->ladder.rung == 0 &&
                    context->ladder.emissions == 1);
            }
            ok = 1;
            goto end;
        }
        for (pass = 0; pass < 12; ++pass) {
            status = ladder_test_step(context, quantum, 16384, output,
                &result, &calls);
            LADDER_REQUIRE(status == SIXEL_OK);
            LADDER_REQUIRE(sixel_repair_measure_read(context, &record)
                == SIXEL_OK && record.ladder.count > 0);
            if (mode == 138) {
                LADDER_REQUIRE(result.audit.outcome == SIXEL_REPAIR_STALLED
                    && record.unresolved == 2 && record.before_sum > 0 &&
                    record.full_selection && record.zero_noise &&
                    record.rescue && context->rescue_position == 2 &&
                    record.ladder.count == 1 && record.ladder.noise_off &&
                    sink.size == 0);
                ok = 1;
                goto end;
            }
            LADDER_REQUIRE(result.audit.outcome == SIXEL_REPAIR_FRAME_EMITTED
                && record.committed && record.committed_gain > 0);
            if (mode == 136) {
                LADDER_REQUIRE(record.ladder.count == 2 &&
                    record.ladder.rung == 1 && record.ladder.reason ==
                    SIXEL_REPAIR_LADDER_NO_PAINT && record.full_selection &&
                    record.zero_noise && record.committed_pixels == 1 &&
                    encoder->accumulation_pixels[0] == 255 &&
                    encoder->accumulation_pixels[3] > 0);
                ok = 1;
                goto end;
            }
            LADDER_REQUIRE(record.ladder.count == 5);
            if (pass == 0) {
                memcpy(thresholds, record.ladder.thresholds,
                    sizeof(thresholds));
            }
            LADDER_REQUIRE(memcmp(thresholds, record.ladder.thresholds,
                sizeof(thresholds)) == 0);
            rung = record.ladder.rung;
            LADDER_REQUIRE(rung >= last_rung && rung < 5 &&
                record.mask_squared == thresholds[rung]);
            last_rung = rung;
            if (rung < 4) {
                LADDER_REQUIRE(record.ladder.emissions == emissions[rung]
                    && emissions[rung] < 2);
                ++emissions[rung];
            } else {
                ++final_packets;
                LADDER_REQUIRE(record.zero_noise && record.full_selection
                    && record.noise_strength == 0 &&
                    context->ladder.noise_off && context->zero_noise);
            }
            if (quantum == 1) {
                LADDER_REQUIRE(calls > count);
            }
        }
        LADDER_REQUIRE(final_packets >= 4);
        if (mode == 137) {
            saved_ladder = context->ladder;
            LADDER_REQUIRE(sixel_progressive_bind_frame(context, source,
                0, 0, 1) == SIXEL_BAD_ARGUMENT &&
                memcmp(&saved_ladder, &context->ladder,
                    sizeof(saved_ladder)) == 0);
            LADDER_REQUIRE(sixel_progressive_bind_frame(context, source,
                0, 0, 2) == SIXEL_OK && context->ladder.allowance == 2 &&
                context->ladder.count == 0 && context->ladder.rung == 0 &&
                context->ladder.emissions == 0 && !context->ladder.noise_off);
        }
        if (run == 0) {
            saved_size = sink.size;
            memcpy(saved, sink.data, saved_size);
        } else {
            LADDER_REQUIRE(saved_size == sink.size &&
                memcmp(saved, sink.data, saved_size) == 0);
        }
        sixel_output_unref(output);
        output = NULL;
        sixel_frame_unref(source);
        source = NULL;
        sixel_progressive_unref(context);
        context = NULL;
        sixel_encoder_unref(encoder);
        encoder = NULL;
    }
    ok = 1;
end:
    free(owned);
    sixel_output_unref(output);
    sixel_frame_unref(source);
    sixel_progressive_unref(context);
    sixel_encoder_unref(encoder);
    return ok ? 0 : 1;
}
