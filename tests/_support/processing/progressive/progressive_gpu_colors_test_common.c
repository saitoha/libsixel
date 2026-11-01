/* SPDX-License-Identifier: MIT */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
#include <float.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sixel.h>
#include "src/encoder.h"
#include "src/progressive.h"
#include "src/dither-policy-bluenoise.h"
#include "src/sixel_decode_pixels.h"

typedef struct gpu_colors_sink {
    sixel_progressive_t *context;
    unsigned char bytes[4096];
    size_t size;
    size_t calls;
    int scanned;
    int applied;
    int separated;
    double strength;
} gpu_colors_sink_t;

/* Reconstruct the shared positional query, then use double arithmetic as
 * the lookup oracle. For these fixtures require a gap larger than 32 float
 * epsilons times max(1, runner-up distance). This bounds float subtraction,
 * squaring and summation error for both candidates, including FMA changes.
 * Exact CPU/GPU packet comparisons below are only valid with this margin. */
static int
gpu_colors_separated(sixel_repair_application_t const *application)
{
    float query[3];
    float const *color;
    double value;
    double distance;
    double best;
    double second;
    double difference;
    size_t position;
    unsigned int candidate;
    unsigned int winner;
    int channel;
    int x;
    int y;

    for (position = 0; position < application->count; ++position) {
        if (!application->selected[position]) continue;
        x = (int)(position % application->width) + application->origin_x;
        y = (int)(position / application->width) + application->origin_y;
        for (channel = 0; channel < 3; ++channel) {
            value = application->source[position * 3 + channel];
            if (position != application->protected_position) {
                value += application->strength * (32.0 / 255.0) *
                    sixel_bluenoise_sample(x, y, application->phase_x,
                        application->phase_y, application->channel_rgb,
                        channel);
            }
            query[channel] = (float)value;
        }
        best = second = DBL_MAX;
        winner = 0;
        for (candidate = 0; candidate < application->color_count;
                ++candidate) {
            color = application->colors[
                application->palette[candidate]].working;
            distance = 0;
            for (channel = 0; channel < 3; ++channel) {
                difference = (double)query[channel] - color[channel];
                distance += difference * difference;
            }
            if (!isfinite(distance)) return 0;
            if (distance < best) {
                second = best;
                best = distance;
                winner = candidate;
            } else if (distance < second) {
                second = distance;
            }
        }
        if (application->color_count > 1 && second - best <=
                32 * FLT_EPSILON * fmax(1, second)) return 0;
        if (application->paint[position] &&
                application->indices[position] != winner) return 0;
    }
    return 1;
}

static int
gpu_colors_write(char *data, int size, void *priv)
{
    gpu_colors_sink_t *sink;

    sink = priv;
    if (size < 0 || (size_t)size > sizeof(sink->bytes) - sink->size) return -1;
    memcpy(sink->bytes + sink->size, data, (size_t)size);
    sink->size += (size_t)size;
    ++sink->calls;
    if (sink->context == NULL) return 0;
    sink->scanned = sink->context->selection != NULL &&
        sink->context->selection->gpu_used;
    sink->applied = sink->context->application.gpu_used;
    sink->separated = gpu_colors_separated(&sink->context->application);
    sink->strength = sink->context->application.strength;
    return 0;
}

/* Decode palette-definition syntax independently of the indexed-packet
 * builder. Palette selections (#N without ';') are not definitions. */
static int
gpu_colors_definitions(gpu_colors_sink_t const *sink, int hls)
{
    size_t i;
    unsigned int count;

    count = 0;
    for (i = 0; i < sink->size; ++i) {
        if (sink->bytes[i] != '#') continue;
        ++i;
        while (i < sink->size && sink->bytes[i] >= '0' &&
                sink->bytes[i] <= '9') ++i;
        if (i >= sink->size || sink->bytes[i] != ';') continue;
        if (i + 2 >= sink->size || sink->bytes[i + 1] !=
                (unsigned char)(hls ? '1' : '2') ||
                sink->bytes[i + 2] != ';') return 0;
        ++count;
    }
    return count != 0;
}

static sixel_frame_t *
gpu_colors_frame(void)
{
    unsigned char const rgba[24] = {
        51, 102, 179, 255, 181, 46, 79, 0, 212, 31, 105, 255,
        26, 209, 59, 255, 232, 194, 33, 255, 171, 40, 143, 0
    };
    sixel_frame_t *frame;
    sixel_frame_interface_t *frame_if;
    sixel_frame_transparency_t transparency;
    unsigned char *owned;

    frame = NULL;
    owned = malloc(sizeof(rgba));
    if (owned == NULL) return NULL;
    memcpy(owned, rgba, sizeof(rgba));
    if (sixel_frame_new(&frame, NULL) != SIXEL_OK ||
            sixel_frame_init(frame, owned, 3, 2, SIXEL_PIXELFORMAT_RGBA8888,
                NULL, 0) != SIXEL_OK) {
        free(owned);
        sixel_frame_unref(frame);
        return NULL;
    }
    frame_if = sixel_frame_as_interface(frame);
    memset(&transparency, 0, sizeof(transparency));
    transparency.transparent = -1;
    transparency.alpha_zero_is_transparent = 1;
    if (frame_if->vtbl->set_transparency(frame_if, &transparency) != SIXEL_OK) {
        sixel_frame_unref(frame);
        return NULL;
    }
    return frame;
}

#define GPU_COLORS_REQUIRE(value) do { \
    if (!(value)) { \
        fprintf(stderr, "GPU colors W%d U%d H%d N%d G%d, line %d\n", \
            workspace, output_space, hls, noise, gpu, __LINE__); \
        goto end; \
    } \
} while (0)

int
test_progressive_gpu_colors_case(int workspace, int output_space, int hls)
{
    static char const *const workspaces[] = {
        "gamma", "linear", "oklab", "cielab", "din99d"
    };
    static int const work_ids[] = {
        SIXEL_COLORSPACE_GAMMA, SIXEL_COLORSPACE_LINEAR,
        SIXEL_COLORSPACE_OKLAB, SIXEL_COLORSPACE_CIELAB, SIXEL_COLORSPACE_DIN99D
    };
    static char const *const outputs[] = {
        "gamma", "linear", "smpte-c", "display-p3"
    };
    static int const output_ids[] = {
        SIXEL_COLORSPACE_GAMMA, SIXEL_COLORSPACE_LINEAR,
        SIXEL_COLORSPACE_SMPTEC, SIXEL_COLORSPACE_GAMMA
    };
    sixel_encoder_t *encoder;
    sixel_progressive_t *context;
    sixel_frame_t *source;
    sixel_output_t *output;
    sixel_repair_gpu_t *probe;
    sixel_repair_gpu_lookup_t lookup;
    sixel_repair_gpu_match_t match;
    sixel_progressive_config_t config;
    sixel_progressive_budget_t budget;
    sixel_progressive_stats_t stats;
    sixel_repair_work_t work;
    sixel_repair_audit_t audit;
    gpu_colors_sink_t sink;
    float query[3] = { 0 };
    unsigned char history[5 * 4 * 3];
    unsigned char cpu_history[5 * 4 * 3];
    unsigned char cpu_packet[4096];
    unsigned char *decoded;
    double before[6];
    double cpu_errors[6];
    size_t cpu_size;
    size_t passes;
    size_t painted;
    size_t position;
    size_t plane_position;
    size_t i;
    unsigned int flags;
    uint64_t revision;
    int width;
    int height;
    int noise;
    int gpu;
    int expected_gpu;
    int definition;
    int ok;
    SIXELSTATUS status;

    encoder = NULL;
    context = NULL;
    source = NULL;
    output = NULL;
    probe = NULL;
    decoded = NULL;
    noise = gpu = ok = 0;
    cpu_size = 0;
    lookup.queries = lookup.palette = query;
    lookup.count = lookup.colors = 1;
    lookup.policy = SIXEL_GPU_POLICY_FORCE;
    GPU_COLORS_REQUIRE(sixel_repair_gpu_new(&probe, NULL) == SIXEL_OK);
    status = sixel_repair_gpu_lookup(probe, &lookup, &match);
    if (status == SIXEL_FEATURE_ERROR) {
        ok = 77;
        goto end;
    }
    GPU_COLORS_REQUIRE(status == SIXEL_OK);
    source = gpu_colors_frame();
    GPU_COLORS_REQUIRE(source != NULL);
    for (i = 0; i < 20; ++i) {
        history[i * 3] = 17;
        history[i * 3 + 1] = 51;
        history[i * 3 + 2] = 85;
    }
    for (i = 0; i < 6; ++i) {
        if (i != 1 && i != 5) {
            memset(history + ((i / 3 + 1) * 5 + i % 3 + 1) * 3, 0, 3);
        }
    }
    definition = output_space == 3 ? SIXEL_PROGRESSIVE_OUTPUT_DISPLAY_P3
                                   : SIXEL_PROGRESSIVE_OUTPUT_LEGACY;
    /* Each pair starts from identical history. These are consistency and
     * wire/commit tests, not perceptual-quality or timing measurements. */
    for (noise = 0; noise < 2; ++noise) {
        for (gpu = 0; gpu < 4; ++gpu) {
            expected_gpu = gpu == 1 || gpu == 2;
            memset(&sink, 0, sizeof(sink));
            GPU_COLORS_REQUIRE(sixel_encoder_new(&encoder, NULL) == SIXEL_OK);
            GPU_COLORS_REQUIRE(sixel_encoder_setopt(encoder,
                SIXEL_OPTFLAG_UPDATE_POLICY, "delta") == SIXEL_OK);
            GPU_COLORS_REQUIRE(sixel_encoder_setopt(encoder,
                SIXEL_OPTFLAG_GPU_POLICY, "off") == SIXEL_OK);
            if (gpu >= 2) {
                /* Four unresolved/selected pixels admit AUTO at four,
                 * but not five. The six-pixel rectangle is not the test. */
                GPU_COLORS_REQUIRE(sixel_encoder_setopt(encoder,
                    SIXEL_OPTFLAG_GPU_POLICY, gpu == 2 ? "auto:P4" : "auto:P5")
                    == SIXEL_OK);
            }
            GPU_COLORS_REQUIRE(sixel_encoder_setopt(encoder,
                SIXEL_OPTFLAG_ALPHA_POLICY, "keep") == SIXEL_OK);
            GPU_COLORS_REQUIRE(sixel_encoder_setopt(encoder,
                SIXEL_OPTFLAG_WORKING_COLORSPACE, workspaces[workspace])
                == SIXEL_OK);
            GPU_COLORS_REQUIRE(sixel_encoder_setopt(encoder,
                SIXEL_OPTFLAG_OUTPUT_COLORSPACE, outputs[output_space])
                == SIXEL_OK);
            GPU_COLORS_REQUIRE(sixel_encoder_setopt(encoder,
                SIXEL_OPTFLAG_PALETTE_TYPE, hls ? "hls" : "rgb") == SIXEL_OK);
            GPU_COLORS_REQUIRE(sixel_encoder_set_accumulation_buffer(encoder,
                history, 5, 4, SIXEL_PIXELFORMAT_RGB888) == SIXEL_OK);
            GPU_COLORS_REQUIRE(sixel_progressive_config_init(&config,
                sizeof(config)) == SIXEL_OK);
            config.stop_distance_auto = 0;
            config.selection_fraction = 1;
            config.noise_strength = noise ? .2 : 0;
            config.noise_phase_x = 11;
            config.noise_phase_y = 29;
            config.max_colors = 2;
            config.gpu_policy = gpu >= 2 ? SIXEL_PROGRESSIVE_GPU_INHERIT
                : gpu ? SIXEL_GPU_POLICY_FORCE : SIXEL_GPU_POLICY_OFF;
            GPU_COLORS_REQUIRE(sixel_progressive_new(&context, encoder,
                &config, NULL) == SIXEL_OK);
            GPU_COLORS_REQUIRE(sixel_progressive_bind_frame(context, source,
                1, 1, 1) == SIXEL_OK);
            sink.context = context;
            GPU_COLORS_REQUIRE(sixel_output_new(&output, gpu_colors_write,
                &sink, NULL) == SIXEL_OK);
            memset(&work, 0, sizeof(work));
            work.pixel_limit = work.distance_limit = 100000;
            GPU_COLORS_REQUIRE(sixel_repair_context_audit(context, &work,
                &audit) == SIXEL_OK && audit.complete &&
                audit.domain_count == 4 && audit.invalid_count == 0);
            memcpy(before, context->errors, sizeof(before));
            revision = encoder->history_revision;
            memset(&budget, 0, sizeof(budget));
            budget.struct_size = sizeof(budget);
            budget.max_pixel_visits = budget.max_distance_evaluations =
                100000;
            budget.max_packet_bytes = sizeof(sink.bytes);
            memset(&stats, 0, sizeof(stats));
            stats.struct_size = sizeof(stats);
            GPU_COLORS_REQUIRE(sixel_progressive_step(context, &budget,
                output, &stats) == SIXEL_OK &&
                stats.outcome == SIXEL_PROGRESSIVE_FRAME_EMITTED &&
                stats.backend == (expected_gpu ? SIXEL_PROGRESSIVE_BACKEND_METAL
                                              : SIXEL_PROGRESSIVE_BACKEND_CPU));
            GPU_COLORS_REQUIRE(stats.working_colorspace == work_ids[workspace]
                && stats.output_colorspace == output_ids[output_space] &&
                stats.output_color_definition == definition &&
                stats.painted_pixels != 0 && sink.calls == 1 &&
                sink.scanned == expected_gpu && sink.applied == expected_gpu
                && sink.separated
                && sink.strength == config.noise_strength &&
                stats.packet_bytes == sink.size &&
                encoder->history_revision == revision + 1 &&
                gpu_colors_definitions(&sink, hls));
            GPU_COLORS_REQUIRE(sixel_decode_sequence_mapped(sink.bytes,
                sink.size, NULL, &decoded, &width, &height, &flags, &passes,
                encoder->allocator) == SIXEL_OK && width == 3 && height == 2
                && passes == 1);
            painted = 0;
            for (i = 0; i < 20; ++i) {
                /* Convert only the bound 3x2 rectangle to packet positions;
                 * an omitted source pixel or outside halo stays untouched. */
                position = 6;
                if (i / 5 >= 1 && i / 5 < 3 && i % 5 >= 1 && i % 5 < 4) {
                    position = (i / 5 - 1) * 3 + i % 5 - 1;
                }
                if (position < 6 && decoded[position * 4 + 3]) {
                    GPU_COLORS_REQUIRE(position != 1 && position != 5 &&
                        memcmp(encoder->accumulation_pixels + i * 3,
                            decoded + position * 4, 3) == 0);
                    ++painted;
                } else {
                    GPU_COLORS_REQUIRE(memcmp(encoder->accumulation_pixels +
                        i * 3, history + i * 3, 3) == 0);
                }
                GPU_COLORS_REQUIRE(encoder->accumulation_valid_mask[i]);
            }
            GPU_COLORS_REQUIRE(painted == stats.painted_pixels);
            memset(&work, 0, sizeof(work));
            work.pixel_limit = work.distance_limit = 100000;
            GPU_COLORS_REQUIRE(sixel_repair_context_audit(context, &work,
                &audit) == SIXEL_OK && audit.complete);
            for (i = 0; i < 6; ++i) {
                if (i == 1 || i == 5) continue;
                plane_position = (i / 3 + 1) * 5 + i % 3 + 1;
                GPU_COLORS_REQUIRE(isfinite(context->errors[i]) &&
                    context->errors[i] <= before[i]);
                if (decoded[i * 4 + 3]) {
                    GPU_COLORS_REQUIRE(context->errors[i] < before[i]);
                }
                GPU_COLORS_REQUIRE(encoder->accumulation_valid_mask[
                    plane_position]);
                if (!gpu) cpu_errors[i] = context->errors[i];
                else GPU_COLORS_REQUIRE(context->errors[i] == cpu_errors[i]);
            }
            if (!gpu) {
                cpu_size = sink.size;
                memcpy(cpu_packet, sink.bytes, cpu_size);
                memcpy(cpu_history, encoder->accumulation_pixels,
                    sizeof(cpu_history));
            } else {
                GPU_COLORS_REQUIRE(sink.size == cpu_size &&
                    memcmp(cpu_packet, sink.bytes, cpu_size) == 0 &&
                    memcmp(cpu_history, encoder->accumulation_pixels,
                        sizeof(cpu_history)) == 0);
            }
            free(decoded);
            decoded = NULL;
            sixel_output_unref(output);
            output = NULL;
            sixel_progressive_unref(context);
            context = NULL;
            sixel_encoder_unref(encoder);
            encoder = NULL;
        }
    }
    ok = 1;
end:
    free(decoded);
    sixel_output_unref(output);
    sixel_progressive_unref(context);
    sixel_frame_unref(source);
    sixel_encoder_unref(encoder);
    sixel_repair_gpu_free(probe);
    return ok == 77 ? 77 : ok ? EXIT_SUCCESS : EXIT_FAILURE;
}

#undef GPU_COLORS_REQUIRE

/* Source preparation for repair must not relax ordinary P3 admission.
 * This rejection precedes GPU submission and needs no Metal device. */
int
test_progressive_gpu_p3_boundary(void)
{
    sixel_encoder_t *encoder;
    sixel_frame_t *source;
    sixel_output_t *output;
    gpu_colors_sink_t sink;
    int ok;

    encoder = NULL;
    source = NULL;
    output = NULL;
    ok = 0;
    memset(&sink, 0, sizeof(sink));
    if (sixel_encoder_new(&encoder, NULL) != SIXEL_OK ||
            sixel_encoder_setopt(encoder, SIXEL_OPTFLAG_UPDATE_POLICY,
                "delta") != SIXEL_OK ||
            sixel_encoder_setopt(encoder, SIXEL_OPTFLAG_OUTPUT_COLORSPACE,
                "display-p3") != SIXEL_OK ||
            sixel_encoder_setopt(encoder, SIXEL_OPTFLAG_GPU_POLICY,
                "auto:P4") != SIXEL_OK) goto end;
    source = gpu_colors_frame();
    if (source == NULL || sixel_output_new(&output, gpu_colors_write, &sink,
            NULL) != SIXEL_OK) goto end;
    ok = sixel_encoder_encode_frame(encoder, source, output) ==
        SIXEL_BAD_ARGUMENT && sink.calls == 0;
end:
    sixel_output_unref(output);
    sixel_frame_unref(source);
    sixel_encoder_unref(encoder);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
