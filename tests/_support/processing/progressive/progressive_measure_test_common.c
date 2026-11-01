/* SPDX-License-Identifier: MIT */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sixel.h>
#include "src/progressive.h"
#include "src/progressive-measure.h"
#include "src/sixel_decode_pixels.h"

typedef struct measure_test_sink {
    unsigned char data[16384];
    size_t size;
    int fail;
} measure_test_sink_t;

static int
measure_test_write(char *data, int size, void *priv)
{
    measure_test_sink_t *sink;

    sink = priv;
    if (sink->fail || size < 0 ||
            (size_t)size > sizeof(sink->data) - sink->size) {
        return -1;
    }
    memcpy(sink->data + sink->size, data, (size_t)size);
    sink->size += (size_t)size;
    return 0;
}

#define MEASURE_REQUIRE(condition) do { \
    if (!(condition)) { \
        fprintf(stderr, "measurement assertion at line %d\n", __LINE__); \
        goto end; \
    } \
} while (0)

int
test_progressive_measure_case(int mode)
{
    sixel_encoder_t *encoder;
    sixel_allocator_t *allocator;
    sixel_progressive_t *context;
    sixel_progressive_config_t config;
    sixel_repair_work_t work;
    sixel_repair_result_t result;
    sixel_repair_measure_record_t record;
    sixel_repair_prediction_t prediction;
    sixel_frame_t *source;
    sixel_output_t *output;
    measure_test_sink_t sink;
    unsigned char black[192] = {0};
    unsigned char target[192];
    unsigned char saved[16384];
    unsigned char *owned;
    unsigned char *decoded;
    char const baseline[] =
        "\033P0;1q\"1;1;32;2#0;2;0;0;0#0!32B\033\\";
    SIXELSTATUS status;
    size_t quantum;
    size_t bytes;
    size_t calls;
    size_t audit_yields;
    size_t saved_size;
    size_t passes;
    unsigned int flags;
    size_t total;
    size_t crossing;
    size_t expected_bands[SIXEL_REPAIR_MEASURE_BANDS];
    uint64_t visits;
    uint64_t distances;
    double before;
    double after;
    double gain;
    double excess;
    double value;
    int width;
    int height;
    int run;
    int i;
    int j;
    int ok;

    allocator = NULL;
    encoder = NULL;
    context = NULL;
    source = NULL;
    output = NULL;
    decoded = owned = NULL;
    saved_size = 0;
    ok = 0;
    quantum = mode == 120 ? 1 : 1048576;
    bytes = mode == 119 || mode == 118 ? 80 : 16384;
    MEASURE_REQUIRE(sixel_allocator_new(&allocator, NULL, NULL, NULL, NULL)
        == SIXEL_OK);
    for (i = 0; i < 192; ++i) {
        target[i] = (unsigned char)(37 * i + 51);
    }
    for (run = 0; run < (mode == 118 ? 2 : 1); ++run) {
        memset(&sink, 0, sizeof(sink));
        memcpy(sink.data, baseline, sizeof(baseline) - 1);
        sink.size = sizeof(baseline) - 1;
        sink.fail = mode == 121;
        MEASURE_REQUIRE(sixel_encoder_new(&encoder, NULL) == SIXEL_OK);
        MEASURE_REQUIRE(sixel_encoder_setopt(encoder,
            SIXEL_OPTFLAG_UPDATE_POLICY, "delta") == SIXEL_OK);
        MEASURE_REQUIRE(sixel_encoder_setopt(encoder,
            SIXEL_OPTFLAG_GPU_POLICY, "off") == SIXEL_OK);
        MEASURE_REQUIRE(sixel_encoder_setopt(encoder,
            SIXEL_OPTFLAG_WORKING_COLORSPACE, "gamma") == SIXEL_OK);
        MEASURE_REQUIRE(sixel_encoder_set_accumulation_buffer(encoder,
            black, 32, 2, SIXEL_PIXELFORMAT_RGB888) == SIXEL_OK);
        MEASURE_REQUIRE(sixel_progressive_config_init(&config,
            sizeof(config)) == SIXEL_OK);
        config.stop_distance_auto = 0;
        config.stop_distance = 0.05;
        config.selection_fraction = 1;
        config.noise_strength = 0;
        config.max_colors = 8;
        MEASURE_REQUIRE(sixel_progressive_new(&context, encoder, &config,
            NULL) == SIXEL_OK);
        if (run == 0) {
            MEASURE_REQUIRE(sixel_repair_measure_enable(context) == SIXEL_OK);
        }
        owned = malloc(sizeof(target));
        MEASURE_REQUIRE(owned != NULL);
        memcpy(owned, target, sizeof(target));
        MEASURE_REQUIRE(sixel_frame_new(&source, NULL) == SIXEL_OK);
        MEASURE_REQUIRE(sixel_frame_init(source, owned, 32, 2,
            SIXEL_PIXELFORMAT_RGB888, NULL, 0) == SIXEL_OK);
        owned = NULL;
        MEASURE_REQUIRE(sixel_progressive_bind_frame(context, source,
            0, 0, 1) == SIXEL_OK);
        MEASURE_REQUIRE(sixel_output_new(&output, measure_test_write,
            &sink, NULL) == SIXEL_OK);
        audit_yields = 0;
        for (calls = 0; calls < 100000; ++calls) {
            memset(&work, 0, sizeof(work));
            work.pixel_limit = work.distance_limit = quantum;
            status = sixel_repair_context_step(context, &work, bytes, output,
                &result);
            if (run == 0) {
                MEASURE_REQUIRE(sixel_repair_measure_read(context, &record)
                    == SIXEL_OK);
                MEASURE_REQUIRE(record.sequence == calls + 1);
                visits = distances = 0;
                for (i = 0; i < SIXEL_REPAIR_MEASURE_PHASES; ++i) {
                    visits += record.call[i].visits;
                    distances += record.call[i].distances;
                    MEASURE_REQUIRE(record.call[i].seconds >= 0);
                }
                MEASURE_REQUIRE(visits == work.pixel_visits &&
                    distances == work.distance_evaluations);
                if (result.audit.outcome == SIXEL_REPAIR_YIELDED) {
                    MEASURE_REQUIRE(!record.committed &&
                        record.committed_gain == 0);
                }
            }
            if (mode == 120 && !record.audit_complete) {
                ++audit_yields;
            } else if (mode == 120) {
                /* Keep one-unit histogram steps, but do not subdivide
                 * unrelated palette fitting and packet preparation. */
                quantum = 1048576;
            }
            if (status != SIXEL_OK ||
                    result.audit.outcome != SIXEL_REPAIR_YIELDED) {
                break;
            }
        }
        if (mode == 121) {
            MEASURE_REQUIRE(status != SIXEL_OK && !record.committed &&
                record.committed_gain == 0 && record.committed_bytes == 0 &&
                record.after_revision > record.before_revision);
            ok = 1;
            goto end;
        }
        MEASURE_REQUIRE(status == SIXEL_OK &&
            result.audit.outcome == SIXEL_REPAIR_FRAME_EMITTED);
        if (run == 1) {
            MEASURE_REQUIRE(sink.size == saved_size &&
                memcmp(saved, sink.data, saved_size) == 0);
            MEASURE_REQUIRE(sixel_repair_measure_read(context, &record)
                == SIXEL_FALSE);
            ok = 1;
            goto end;
        }
        MEASURE_REQUIRE(record.audit_complete && record.committed &&
            record.proposal_complete && record.eligible == 64);
        total = 0;
        for (i = 0; i < SIXEL_REPAIR_MEASURE_BANDS; ++i) {
            total += record.bands[i];
        }
        MEASURE_REQUIRE(total == 64 && record.sample_count == 64);
        total = 0;
        for (i = 0; i < SIXEL_REPAIR_MEASURE_BANDS; ++i) {
            total += record.prediction_model.bands[i].population;
            MEASURE_REQUIRE(record.prediction_model.bands[i].population ==
                record.bands[i]);
        }
        MEASURE_REQUIRE(total == 64);
        /* C99 needs an explicit read-only view of the array member. */
        MEASURE_REQUIRE(sixel_repair_predict(&record.prediction_model,
            (float const (*)[3])record.palette, record.palette_count,
            record.mask_squared, 256, &prediction) == SIXEL_OK &&
            prediction.exact &&
            fabs(prediction.gain - record.proposal_gain) < 1e-10 &&
            prediction.painted == record.proposal_pixels);
        MEASURE_REQUIRE(sixel_decode_sequence_mapped(sink.data, sink.size,
            NULL, &decoded, &width, &height, &flags, &passes, allocator)
                == SIXEL_OK);
        MEASURE_REQUIRE(width == 32 && height == 2 && passes == 2);
        gain = excess = 0;
        crossing = 0;
        for (i = 0; i < 64; ++i) {
            before = after = 0;
            for (j = 0; j < 3; ++j) {
                value = (float)(target[i * 3 + j] / 255.0);
                before += value * value;
                value -= (float)(decoded[i * 4 + j] / 255.0);
                after += value * value;
            }
            gain += before - after;
            excess += fmax(0, before - 0.0025) - fmax(0, after - 0.0025);
            crossing += before > 0.0025 && after <= 0.0025;
        }
        MEASURE_REQUIRE(fabs(gain - record.committed_gain) < 0.00001 &&
            fabs(excess - record.committed_excess_gain) < 0.00001 &&
            crossing == record.resolved_pixels);
        total = 0;
        for (i = 0; i < SIXEL_REPAIR_MEASURE_BANDS; ++i) {
            for (j = 0; j < SIXEL_REPAIR_MEASURE_BANDS; ++j) {
                total += record.transitions[i][j];
                MEASURE_REQUIRE(j <= i || record.transitions[i][j] == 0);
            }
        }
        MEASURE_REQUIRE(total == result.painted &&
            record.committed_bytes == result.packet_bytes);
        if (mode == 119) {
            MEASURE_REQUIRE(record.fitting_retries > 0 &&
                record.proposal_gain > record.committed_gain &&
                record.proposal_bytes > bytes &&
                record.committed_bytes <= bytes);
        }
        if (mode == 120) {
            MEASURE_REQUIRE(audit_yields >= 63 && calls > audit_yields);
        }
        if (mode == 123) {
            memcpy(expected_bands, record.bands, sizeof(expected_bands));
            for (i = 0; i < SIXEL_REPAIR_MEASURE_BANDS; ++i) {
                for (j = 0; j < SIXEL_REPAIR_MEASURE_BANDS; ++j) {
                    expected_bands[i] -= record.transitions[i][j];
                    expected_bands[j] += record.transitions[i][j];
                }
            }
            before = record.before_sum - record.committed_gain;
            for (calls = 0; calls < 1000; ++calls) {
                memset(&work, 0, sizeof(work));
                work.pixel_limit = work.distance_limit = 1;
                MEASURE_REQUIRE(sixel_repair_context_step(context, &work,
                    bytes, output, &result) == SIXEL_OK);
                MEASURE_REQUIRE(sixel_repair_measure_read(context, &record)
                    == SIXEL_OK && !record.committed);
                if (record.audit_complete) {
                    break;
                }
            }
            MEASURE_REQUIRE(calls < 1000 &&
                memcmp(expected_bands, record.bands, sizeof(expected_bands))
                    == 0 && fabs(record.before_sum - before) < 0.00001);
        }
        if (mode == 122) {
            MEASURE_REQUIRE(sixel_progressive_bind_frame(context, source,
                0, 0, 1) != SIXEL_OK);
            MEASURE_REQUIRE(sixel_repair_measure_read(context, &record)
                == SIXEL_OK && record.committed);
            MEASURE_REQUIRE(sixel_progressive_bind_frame(context, source,
                0, 0, 2) == SIXEL_OK);
            MEASURE_REQUIRE(sixel_repair_measure_read(context, &record)
                == SIXEL_FALSE);
        }
        saved_size = sink.size;
        memcpy(saved, sink.data, saved_size);
        free(decoded);
        decoded = NULL;
        sixel_output_unref(output);
        output = NULL;
        sixel_progressive_unref(context);
        context = NULL;
        sixel_frame_unref(source);
        source = NULL;
        sixel_encoder_unref(encoder);
        encoder = NULL;
    }
    ok = 1;
end:
    free(owned);
    free(decoded);
    sixel_output_unref(output);
    sixel_progressive_unref(context);
    sixel_frame_unref(source);
    sixel_encoder_unref(encoder);
    sixel_allocator_unref(allocator);
    return ok ? 0 : 1;
}
