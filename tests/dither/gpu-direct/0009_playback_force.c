/* SPDX-License-Identifier: MIT */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sixel.h>
#include <6cells.h>

#define PLAYBACK_WIDTH 96
#define PLAYBACK_HEIGHT 48
#define PLAYBACK_FRAMES 4

static int g_playback_failures;
static int g_playback_serial;
static sixel_playback_t *g_playback;

static void
playback_force_done(sixel_playback_result_t const *result, void *context)
{
    sixel_playback_pipeline_sample_t sample;
    unsigned int required;

    (void)context;
    /* Apply and encode must run as separate pipelined stages, so a direct
     * backend never collapses them onto the synchronous controller. */
    required = SIXEL_PLAYBACK_PIPELINE_INDEPENDENT |
        SIXEL_PLAYBACK_PIPELINE_RESOLVE | SIXEL_PLAYBACK_PIPELINE_ENCODE;
    memset(&sample, 0, sizeof(sample));
    sample.struct_size = sizeof(sample);
    if (SIXEL_SUCCEEDED(result->status) &&
            (SIXEL_FAILED(sixel_playback_get_pipeline_sample(g_playback,
                result->sequence, &sample)) ||
             (sample.valid & required) != required)) {
        g_playback_serial++;
    }
    if (SIXEL_FAILED(result->status)) {
        fprintf(stderr, "frame %llu failed: %s\n",
            (unsigned long long)result->sequence,
            sixel_helper_format_error(result->status));
        g_playback_failures++;
    }
}

static SIXELSTATUS
playback_force_sink(char *data, int size, void *priv)
{
    (void)data;
    (void)size;
    (void)priv;
    return SIXEL_OK;
}

/* Queued 6delta frames under a forced GPU policy must encode on every
 * backend that applies directly, not only on one with prepared plans. */
int
test_gpu_direct_0009_playback_force(int argc, char **argv)
{
    sixel_encoder_t *encoder;
    sixel_output_t *output;
    sixel_playback_t *playback;
    sixel_playback_config_t config;
    sixel_playback_options_t options;
    sixel_frame_t *frame;
    sixel_gpu_statistics_t before;
    sixel_gpu_statistics_t after;
    unsigned char *pixels;
    size_t size;
    size_t i;
    int n;
    int result;
    SIXELSTATUS status;

    (void)argc;
    (void)argv;
    encoder = NULL;
    output = NULL;
    playback = NULL;
    result = 1;
    g_playback_failures = 0;
    g_playback_serial = 0;
    g_playback = NULL;
    size = (size_t)PLAYBACK_WIDTH * PLAYBACK_HEIGHT * 3;
    if (SIXEL_FAILED(sixel_encoder_new(&encoder, NULL)) ||
            SIXEL_FAILED(sixel_encoder_setopt(encoder,
                SIXEL_OPTFLAG_GPU_POLICY, "force")) ||
            SIXEL_FAILED(sixel_encoder_setopt(encoder,
                SIXEL_OPTFLAG_DIFFUSION, "none")) ||
            SIXEL_FAILED(sixel_encoder_setopt(encoder,
                SIXEL_OPTFLAG_LUT_POLICY, "none")) ||
            SIXEL_FAILED(sixel_encoder_setopt(encoder,
                SIXEL_OPTFLAG_UPDATE_POLICY, "delta:threshold=4")) ||
            SIXEL_FAILED(sixel_output_new(&output, playback_force_sink,
                NULL, NULL)) ||
            SIXEL_FAILED(sixel_playback_config_init(&config,
                sizeof(config)))) {
        fprintf(stderr, "encoder setup failed\n");
        goto end;
    }
    config.capacity = 2;
    config.done = playback_force_done;
    status = sixel_playback_new(&playback, encoder, output, &config, NULL);
    if (SIXEL_FAILED(status)) {
        fprintf(stderr, "playback setup failed: %s\n",
            sixel_helper_format_error(status));
        goto end;
    }
    g_playback = playback;
    if (SIXEL_FAILED(sixel_gpu_get_statistics(&before))) goto end;
    for (n = 0; n < PLAYBACK_FRAMES; ++n) {
        pixels = malloc(size);
        if (pixels == NULL) goto end;
        for (i = 0; i < size; ++i) {
            pixels[i] = (unsigned char)((i * 7 + (size_t)n * 40) & 0xff);
        }
        frame = NULL;
        status = sixel_frame_new(&frame, NULL);
        if (SIXEL_SUCCEEDED(status)) {
            status = sixel_frame_init(frame, pixels, PLAYBACK_WIDTH,
                PLAYBACK_HEIGHT, SIXEL_PIXELFORMAT_RGB888, NULL, -1);
        }
        if (SIXEL_FAILED(status)) {
            free(pixels);
            if (frame != NULL) sixel_frame_unref(frame);
            goto end;
        }
        status = sixel_playback_options_init(&options, sizeof(options));
        if (SIXEL_SUCCEEDED(status)) {
            options.flags = SIXEL_PLAYBACK_OPTION_ORIGIN;
            status = sixel_playback_submit(playback, frame,
                SIXEL_PLAYBACK_WAIT, -1, &options);
        }
        sixel_frame_unref(frame);
        if (SIXEL_FAILED(status)) {
            fprintf(stderr, "frame %d was not admitted: %s\n", n,
                sixel_helper_format_error(status));
            goto end;
        }
    }
    status = sixel_playback_drain(playback, -1);
    if (SIXEL_FAILED(status) || g_playback_failures != 0) {
        fprintf(stderr, "queued frames failed: %s\n",
            sixel_helper_format_error(status));
        goto end;
    }
    if (SIXEL_FAILED(sixel_gpu_get_statistics(&after))) goto end;
    if (after.dispatches <= before.dispatches) {
        fprintf(stderr, "queued frames never reached the GPU\n");
        goto end;
    }
    if (g_playback_serial != 0) {
        fprintf(stderr, "%d frames ran apply and encode serially\n",
            g_playback_serial);
        goto end;
    }
    result = 0;
end:
    if (playback != NULL) sixel_playback_unref(playback);
    if (output != NULL) sixel_output_unref(output);
    if (encoder != NULL) sixel_encoder_unref(encoder);
    return result;
}
