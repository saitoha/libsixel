/* SPDX-License-Identifier: MIT */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "src/encoder.h"
#include "src/presentation.h"
#include "src/output.h"
#include "src/sixel_decode_pixels.h"

#define VIDEO_REQUIRE(value) do { \
    if (!(value)) { \
        fprintf(stderr, "video case %d, line %d\n", mode, __LINE__); \
        goto end; \
    } \
} while (0)

typedef struct video_test_sink {
    unsigned char data[8192];
    size_t size;
    double time;
    double tick;
    int fail;
    sixel_repair_context_t *repair;
    int gpu_scan;
    int gpu_apply;
} video_test_sink_t;

static double
video_test_now(void *priv)
{
    video_test_sink_t *sink;

    sink = priv;
    sink->time += sink->tick;
    return sink->time;
}

static void
video_test_wait(unsigned int usec, void *priv)
{
    ((video_test_sink_t *)priv)->time += usec / 1000000.0;
}

static int
video_test_write(char *data, int size, void *priv)
{
    video_test_sink_t *sink;

    sink = priv;
    if (sink->repair != NULL) {
        sink->gpu_scan |= sink->repair->selection != NULL &&
            sink->repair->selection->gpu_used;
        sink->gpu_apply |= sink->repair->application.gpu_used;
    }
    if (sink->fail || size < 0 ||
            (size_t)size > sizeof(sink->data) - sink->size) return -1;
    memcpy(sink->data + sink->size, data, (size_t)size);
    sink->size += size;
    return 0;
}

int
test_progressive_video_case(int mode)
{
    static char const baseline[] =
        "\0337\033P0;1q\"1;1;6;4#0;2;100;0;0#1;2;0;0;0"
        "#1!6N$#0@\033\\";
    sixel_encoder_t *encoder;
    sixel_allocator_t *allocator;
    sixel_frame_t *source;
    sixel_output_t *output;
    sixel_presentation_t presentation;
    sixel_repair_context_t guard;
    sixel_repair_work_t work;
    sixel_repair_gpu_t *gpu;
    sixel_repair_gpu_lookup_t probe;
    sixel_repair_gpu_match_t match;
    sixel_palette_emitted_color_t color;
    video_test_sink_t sink;
    unsigned char retained[72] = {255, 0, 0};
    unsigned char paint[2] = {1, 0};
    unsigned char domain[2] = {1, 1};
    unsigned char indices[2] = {0, 0};
    unsigned int palette[1] = {0};
    double errors[2] = {.1, .1};
    double luma[2] = {.5, .5};
    double after[2];
    float *pixels;
    unsigned char *decoded;
    size_t passes;
    size_t initial_size;
    unsigned int flags;
    int width;
    int height;
    int owned;
    int done;
    int accepted;
    int i;
    int ok;
    SIXELSTATUS status;

    gpu = NULL;
    encoder = NULL;
    allocator = NULL;
    source = NULL;
    output = NULL;
    pixels = NULL;
    decoded = NULL;
    owned = ok = 0;
    memset(&presentation, 0, sizeof(presentation));
    memset(&guard, 0, sizeof(guard));
    memset(&sink, 0, sizeof(sink));
    memset(&color, 0, sizeof(color));
    VIDEO_REQUIRE(sixel_allocator_new(&allocator, NULL, NULL, NULL, NULL)
        == SIXEL_OK);
    if (mode == 191) {
        VIDEO_REQUIRE(sixel_repair_gpu_new(&gpu, allocator) == SIXEL_OK);
        probe.queries = probe.palette = color.working;
        probe.count = probe.colors = 1;
        probe.policy = SIXEL_GPU_POLICY_FORCE;
        status = sixel_repair_gpu_lookup(gpu, &probe, &match);
        if (status == SIXEL_FEATURE_ERROR) {
            ok = 77;
            goto end;
        }
        VIDEO_REQUIRE(status == SIXEL_OK);
    }
    VIDEO_REQUIRE(sixel_encoder_new(&encoder, allocator) == SIXEL_OK);
    VIDEO_REQUIRE(sixel_encoder_setopt(encoder, 'Z', "delta:P1:S0:N2:M0")
        == SIXEL_OK);
    /* Test the actual paired scheduler synchronously with a deterministic
     * clock. Transport ordering/failure has a separate real-worker suite. */
    VIDEO_REQUIRE(sixel_encoder_setopt(encoder, 'g', NULL) == SIXEL_OK);
    VIDEO_REQUIRE(sixel_encoder_setopt(encoder, 'G',
        mode == 191 ? "force" : "off") == SIXEL_OK);
    VIDEO_REQUIRE(sixel_encoder_setopt(encoder, 'W', "gamma") == SIXEL_OK);
    VIDEO_REQUIRE(sixel_output_new(&output, video_test_write, &sink,
        allocator) == SIXEL_OK);
    VIDEO_REQUIRE(sixel_encoder_operation_enter(encoder,
        SIXEL_ENCODER_OPERATION_NORMAL) == SIXEL_OK);
    owned = 1;
    VIDEO_REQUIRE(sixel_presentation_init_owned(&presentation, encoder,
        output, -1) == SIXEL_OK);
    if (mode == 191) sink.repair = presentation.repair;
    encoder->fignore_delay = 0;
    presentation.now = video_test_now;
    presentation.wait = video_test_wait;
    presentation.clock_priv = &sink;
    presentation.saved = 1;
    presentation.video_pair = 1;
    VIDEO_REQUIRE(sixel_presentation_capture_owned(&presentation, 6, 4)
        == SIXEL_OK);
    presentation.ordinary_paint[0] = 1;
    if (mode == 156) memset(presentation.ordinary_paint, 1, 24);
    sixel_encoder_operation_leave(encoder);
    owned = 0;
    VIDEO_REQUIRE(sixel_encoder_set_accumulation_buffer(encoder, retained,
        6, 4, SIXEL_PIXELFORMAT_RGB888) == SIXEL_OK);
    pixels = malloc(72 * sizeof(float));
    VIDEO_REQUIRE(pixels != NULL);
    for (i = 0; i < 72; ++i) pixels[i] = .5f;
    VIDEO_REQUIRE(sixel_frame_new(&source, allocator) == SIXEL_OK);
    VIDEO_REQUIRE(sixel_frame_init(source, (unsigned char *)pixels, 6, 4,
        SIXEL_PIXELFORMAT_RGBFLOAT32, NULL, 0) == SIXEL_OK);
    pixels = NULL;
    VIDEO_REQUIRE(sixel_encoder_operation_enter(encoder,
        SIXEL_ENCODER_OPERATION_NORMAL) == SIXEL_OK);
    owned = 1;
    VIDEO_REQUIRE(sixel_presentation_bind_owned(&presentation, source)
        == SIXEL_OK);
    sixel_encoder_operation_leave(encoder);
    owned = 0;
    if (mode == 153) {
        /* Correcting half of a uniform offset creates a false edge even
         * though that pixel's individual color error becomes zero. */
        guard.source = presentation.repair->source;
        guard.count = 2;
        guard.width = 2;
        guard.height = 1;
        guard.domain = domain;
        guard.video_error = errors;
        guard.video_luma = luma;
        guard.video_after = after;
        guard.paint = paint;
        guard.indices = indices;
        guard.workspace = SIXEL_COLORSPACE_GAMMA;
        color.working[0] = color.working[1] = color.working[2] = .5f;
        guard.application.colors = &color;
        guard.application.palette = palette;
        done = accepted = 0;
        for (i = 0; i < 20 && !done; ++i) {
            memset(&work, 0, sizeof(work));
            work.pixel_limit = work.distance_limit = 1;
            VIDEO_REQUIRE(sixel_repair_video_guard(&guard, &work, &done,
                &accepted) == SIXEL_OK);
        }
        VIDEO_REQUIRE(done && !accepted &&
            guard.video_after_loss > guard.video_before_loss);
        ok = 1;
        goto end;
    }
    memcpy(sink.data, baseline, sizeof(baseline) - 1);
    sink.size = sizeof(baseline) - 1;
    initial_size = sink.size;
    VIDEO_REQUIRE(sixel_presentation_before_frame(&presentation) == SIXEL_OK);
    if (mode == 154) sink.tick = .003;
    if (mode == 155) sink.fail = 1;
    status = sixel_presentation_after_frame(&presentation, 4, 1);
    if (mode == 155) {
        VIDEO_REQUIRE(status != SIXEL_OK && !encoder->accumulation_valid);
        ok = 1;
        goto end;
    }
    VIDEO_REQUIRE(status == SIXEL_OK && !presentation.finished);
    if (mode == 154 || mode == 156) {
        VIDEO_REQUIRE(sink.size == initial_size && presentation.passes == 1);
        if (mode == 156) {
            VIDEO_REQUIRE(sixel_presentation_finish(&presentation)
                == SIXEL_OK && presentation.passes == 2);
        }
        ok = 1;
        goto end;
    }
    VIDEO_REQUIRE(presentation.passes == 2 && sink.size > initial_size);
    VIDEO_REQUIRE(mode != 191 || (sink.gpu_scan && sink.gpu_apply));
    VIDEO_REQUIRE(fabs(sink.time - .04) < .000002);
    VIDEO_REQUIRE(sixel_decode_sequence_mapped(sink.data, sink.size, NULL,
        &decoded, &width, &height, &flags, &passes, allocator) == SIXEL_OK);
    VIDEO_REQUIRE(passes == 2 && width == 6 && height == 4);
    VIDEO_REQUIRE(decoded[0] == 255 && !decoded[1] && !decoded[2]);
    VIDEO_REQUIRE(encoder->accumulation_pixels[0] == 255 &&
        encoder->accumulation_pixels[1] == 0);
    for (i = 1; i < 24; ++i) {
        VIDEO_REQUIRE(decoded[i * 4] >= 127 && decoded[i * 4] <= 128 &&
            decoded[i * 4 + 1] == decoded[i * 4] &&
            decoded[i * 4 + 2] == decoded[i * 4]);
    }
    if (mode == 151 || mode == 152) {
        presentation.repair->video_age[1] = 7;
        presentation.repair->video_age[2] = 9;
        pixels = (float *)sixel_frame_get_pixels(source);
        pixels[6] = pixels[7] = pixels[8] = .75f;
        pixels = NULL;
        VIDEO_REQUIRE(sixel_encoder_operation_enter(encoder,
            SIXEL_ENCODER_OPERATION_NORMAL) == SIXEL_OK);
        owned = 1;
        VIDEO_REQUIRE(sixel_presentation_bind_owned(&presentation, source)
            == SIXEL_OK);
        VIDEO_REQUIRE(presentation.repair->video_age[1] == 8 &&
            presentation.repair->video_age[2] == 0);
        sixel_encoder_operation_leave(encoder);
        owned = 0;
        if (mode == 152) {
            VIDEO_REQUIRE(sixel_presentation_after_frame(&presentation, 4, 1)
                == SIXEL_OK && presentation.passes == 2);
            VIDEO_REQUIRE(encoder->accumulation_pixels[6] >= 190 &&
                encoder->accumulation_pixels[6] <= 192);
        }
    }
    ok = 1;
end:
    if (owned) sixel_encoder_operation_leave(encoder);
    (void)sixel_presentation_shutdown(&presentation);
    if (encoder != NULL && sixel_encoder_operation_enter(encoder,
            SIXEL_ENCODER_OPERATION_NORMAL) == SIXEL_OK) {
        sixel_presentation_dispose_owned(&presentation);
        sixel_encoder_operation_leave(encoder);
    }
    sixel_allocator_free(allocator, decoded);
    free(pixels);
    sixel_frame_unref(source);
    sixel_output_unref(output);
    sixel_encoder_unref(encoder);
    sixel_allocator_unref(allocator);
    sixel_repair_gpu_free(gpu);
    return ok == 77 ? 77 : ok ? 0 : 1;
}
