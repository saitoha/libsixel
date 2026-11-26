/* SPDX-License-Identifier: MIT */
#include "config.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "src/palette-kmeans-gpu.h"

int
test_kmgpu_available(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return sixel_kmeans_gpu_is_available() ? EXIT_SUCCESS : 77;
}

int
test_kmgpu_resident_available(int argc, char **argv)
{
#if defined(HAVE_METAL) || defined(HAVE_VULKAN)
    return test_kmgpu_available(argc, argv);
#else
    (void)argc;
    (void)argv;
    return 77;
#endif
}

#if defined(HAVE_METAL) || defined(HAVE_VULKAN)
typedef struct resident_sink {
    uint64_t hash;
    size_t length;
} resident_sink_t;

static int
resident_write(char *bytes, int length, void *context)
{
    resident_sink_t *sink;
    int i;

    sink = context;
    for (i = 0; i < length; ++i) {
        sink->hash = (sink->hash ^ (unsigned char)bytes[i]) *
            UINT64_C(1099511628211);
    }
    sink->length += (size_t)length;
    return length;
}
#endif

static int
test_resident_playback(int heckbert, int ward)
{
#if defined(HAVE_METAL) || defined(HAVE_VULKAN)
    static struct {
        int option;
        char const *value;
    } const options[] = {
        { SIXEL_OPTFLAG_GPU_POLICY, "force" },
        /* Keep body composition on the CPU when counting palette submits. */
        { SIXEL_OPTFLAG_THREADING_POLICY, "greedy:global_budget=1" },
        { SIXEL_OPTFLAG_PRECISION_POLICY, "8bit" },
        { SIXEL_OPTFLAG_SAMPLING_POLICY, "adaptive-grid:solid=off" },
        { SIXEL_OPTFLAG_LUT_POLICY, "6bit" },
        { SIXEL_OPTFLAG_DIFFUSION, "none" },
        { SIXEL_OPTFLAG_COLORS, "32" },
        { SIXEL_OPTFLAG_COVER_POLICY, "edges:cover_mode=soft:cover_grow=1" },
        { SIXEL_OPTFLAG_SNAP_POLICY, "reversible:rate=0.5" }
    };
    unsigned char rgb[32u * 32u * 3u];
    sixel_encoder_t *encoder;
    sixel_output_t *direct;
    sixel_output_t *queued;
    sixel_frame_t *frame;
    sixel_playback_t *playback;
    sixel_playback_config_t config;
    resident_sink_t sink[2];
    sixel_gpu_statistics_t before, after;
    size_t i;
    SIXELSTATUS status;
    int result;
#endif

    (void)heckbert;
    (void)ward;
#if !defined(HAVE_METAL) && !defined(HAVE_VULKAN)
    return 77;
#else
    if (!sixel_kmeans_gpu_is_available())
        return 77;
    encoder = NULL;
    direct = NULL;
    queued = NULL;
    frame = NULL;
    playback = NULL;
    memset(sink, 0, sizeof(sink));
    result = EXIT_FAILURE;
    for (i = 0; i < sizeof(rgb) / 3u; ++i) {
        rgb[3u*i] = (unsigned char)(i * 7u);
        rgb[3u*i+1] = (unsigned char)(i / 5u);
        rgb[3u*i+2] = (unsigned char)(i / 13u + i * 29u);
    }

#define RESIDENT_CHECK(call) do { \
    status = (call); \
    if (status != SIXEL_OK) { \
        fprintf(stderr, "%s: 0x%x %s\n", #call, (unsigned int)status, \
                sixel_helper_get_additional_message()); \
        goto end; \
    } \
} while (0)
    RESIDENT_CHECK(sixel_encoder_new(&encoder, NULL));
    RESIDENT_CHECK(sixel_encoder_setopt(encoder, SIXEL_OPTFLAG_QUANTIZE_MODEL,
        heckbert ? "heckbert" :
            "kmeans:backend=gpu:inittype=pca:binbits=5:iter=6"));
    RESIDENT_CHECK(sixel_encoder_setopt(encoder, SIXEL_OPTFLAG_BINNING_POLICY,
                                        heckbert ? "auto" : "hard"));
    if (heckbert) {
        RESIDENT_CHECK(sixel_encoder_setopt(encoder, SIXEL_OPTFLAG_SELECT_COLOR,
                                            ward ? "center" : "histogram"));
    }
    if (ward) {
        RESIDENT_CHECK(sixel_encoder_setopt(encoder, SIXEL_OPTFLAG_MERGE_POLICY,
                                            "ward"));
    }
    for (i = 0; i < sizeof(options)/sizeof(options[0]); ++i)
        RESIDENT_CHECK(sixel_encoder_setopt(encoder, options[i].option,
                                   options[i].value));
    RESIDENT_CHECK(sixel_output_new(&direct, resident_write, &sink[0], NULL));
    RESIDENT_CHECK(sixel_output_new(&queued, resident_write, &sink[1], NULL));
    RESIDENT_CHECK(sixel_frame_new(&frame, NULL));
    RESIDENT_CHECK(sixel_frame_init_borrowed(frame, rgb, 32, 32,
                                   SIXEL_PIXELFORMAT_RGB888, NULL, -1));
    RESIDENT_CHECK(sixel_gpu_get_statistics(&before));
    RESIDENT_CHECK(sixel_encoder_encode_frame(encoder, frame, direct));
    RESIDENT_CHECK(sixel_gpu_get_statistics(&after));
    if (after.dispatches != before.dispatches + 1u) {
        fprintf(stderr, "direct resident encode must submit once.\n");
        goto end;
    }
    /* Reuse the same caller-owned frame after the direct call. */
    RESIDENT_CHECK(sixel_playback_config_init(&config, sizeof(config)));
    RESIDENT_CHECK(sixel_playback_new(&playback, encoder, queued,
                                      &config, NULL));
    RESIDENT_CHECK(sixel_gpu_get_statistics(&before));
    RESIDENT_CHECK(sixel_playback_submit(playback, frame, SIXEL_PLAYBACK_WAIT,
                                         -1, NULL));
    RESIDENT_CHECK(sixel_playback_drain(playback, -1));
    RESIDENT_CHECK(sixel_gpu_get_statistics(&after));
    if (after.dispatches != before.dispatches + 1u) {
        fprintf(stderr, "queued resident encode must submit once.\n");
        goto end;
    }
#undef RESIDENT_CHECK
    if (sink[0].length && sink[0].length == sink[1].length &&
            sink[0].hash == sink[1].hash) {
        result = EXIT_SUCCESS;
    } else {
        fprintf(stderr, "direct and queued output differ: %zu vs %zu bytes\n",
                sink[0].length, sink[1].length);
    }
end:
    sixel_playback_unref(playback);
    sixel_frame_unref(frame);
    sixel_output_unref(queued);
    sixel_output_unref(direct);
    sixel_encoder_unref(encoder);
    return result;
#endif
}

int
test_kmgpu_resident_playback(int argc, char **argv)
{
    return test_resident_playback(0, argc > 1 &&
        strcmp(argv[1], "ward") == 0);
}

int
test_heckbert_resident_playback(int argc, char **argv)
{
    return test_resident_playback(1, argc > 1 &&
        strcmp(argv[1], "ward") == 0);
}
