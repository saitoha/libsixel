/* SPDX-License-Identifier: MIT */
/* Integration fixture: inspect builder events, not wall-clock thresholds. */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sixel.h>
#include "src/compat_stub.h"
#include "src/encoder.h"
#include "src/threading.h"

/* Hold the main thread after launch so the worker must make progress
 * independently. Image size and host scheduling must not create the order. */
struct fhedt_build_gate {
    sixel_mutex_t mutex;
    sixel_cond_t changed;
    int finished;
    int observed;
};

static void
fhedt_build_finished(void *context)
{
    struct fhedt_build_gate *gate;

    gate = context;
    sixel_mutex_lock(&gate->mutex);
    gate->finished = 1;
    sixel_cond_broadcast(&gate->changed);
    sixel_mutex_unlock(&gate->mutex);
}

static void
fhedt_build_launched(void *context)
{
    struct fhedt_build_gate *gate;

    gate = context;
    sixel_mutex_lock(&gate->mutex);
    while (!gate->finished) {
        if (sixel_cond_timedwait(&gate->changed, &gate->mutex, 5000) != 0) {
            break;
        }
    }
    gate->observed = gate->finished;
    sixel_mutex_unlock(&gate->mutex);
}

static int
fhedt_build_sink(char *data, int size, void *context)
{
    (void)data;
    (void)context;
    return size;
}

int
fhedt_build_trace(char const *path, int overlap)
{
    FILE *file;
    char line[2048];
    char *field;
    unsigned long long thread;
    unsigned long long main_thread;
    unsigned long long builder_thread;
    int builders;
    int workers;
    int main_finish;
    int early_builder;
    int minimum_workers;

    file = sixel_compat_fopen(path, "rb");
    if (file == NULL) {
        perror("open timeline log");
        return EXIT_FAILURE;
    }
    builders = workers = main_finish = early_builder = 0;
    main_thread = builder_thread = 0;
    while (fgets(line, sizeof(line), file) != NULL) {
        field = strstr(line, "\"thread\":");
        if (field == NULL ||
                sixel_compat_sscanf(field, "\"thread\":%llu", &thread) != 1) {
            continue;
        }
        if (strstr(line, "\"event\":\"encode_begin\"") != NULL) {
            main_thread = thread;
        }
        if (strstr(line, "\"worker\":\"fhedt\"") != NULL &&
                strstr(line, "\"role\":\"fhedt\"") != NULL &&
                strstr(line, "\"event\":\"builder-start\"") != NULL) {
            ++builders;
            builder_thread = thread;
            early_builder = !main_finish;
        }
        if (thread == main_thread &&
                strstr(line, "\"worker\":\"worker\"") != NULL &&
                strstr(line, "\"role\":\"filter\"") != NULL &&
                strstr(line, "\"event\":\"colorspace-finish\"") != NULL) {
            main_finish = 1;
        }
        if (strstr(line, "\"worker\":\"dither\"") != NULL &&
                strstr(line, "\"role\":\"worker\"") != NULL &&
                strstr(line, "\"event\":\"start\"") != NULL) {
            ++workers;
        }
    }
    fclose(file);
    /* Grid construction remains shared even when the compiled dither path
     * is serial. Keep the independent builder/filter ordering assertion. */
    minimum_workers = 2;
#if defined(__PCC__) || defined(__TINYC__)
    minimum_workers = 1;
#endif
    if (builders != 1 || workers < minimum_workers || (overlap &&
            (!main_thread || !main_finish || !early_builder ||
             builder_thread == main_thread))) {
        fprintf(stderr, "builders=%d workers=%d finish=%d early=%d\n",
            builders, workers, main_finish, early_builder);
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

int
fhedt_build_encode(int quality, int overlap, char const *path)
{
    struct fhedt_build_gate gate;
    struct sixel_encoder_palette_test_hooks hooks;
    int gate_ready;
    sixel_encoder_t *encoder;
    sixel_output_t *output;
    sixel_frame_t *frame;
    unsigned char *pixels;
    unsigned int random;
    int size;
    int i;
    int ok;

    memset(&gate, 0, sizeof(gate));
    memset(&hooks, 0, sizeof(hooks));
    gate_ready = 0;
    encoder = NULL;
    output = NULL;
    frame = NULL;
    size = overlap ? 1024 : 256;
    pixels = malloc((size_t)size * (size_t)size * 3U);
    if (pixels == NULL) {
        return EXIT_FAILURE;
    }
    random = 1U;
    for (i = 0; i < size * size * 3; ++i) {
        random = (unsigned int)((uint64_t)random * 1664525U
            + 1013904223U);
        pixels[i] = (unsigned char)(random >> 24);
    }
    ok = 0;
    if (SIXEL_FAILED(sixel_encoder_new(&encoder, NULL)) ||
            SIXEL_FAILED(sixel_output_new(&output, fhedt_build_sink,
                NULL, NULL)) || SIXEL_FAILED(sixel_frame_new(&frame, NULL))) {
        goto end;
    }
    if (overlap) {
        if (sixel_mutex_init(&gate.mutex) != 0) {
            goto end;
        }
        if (sixel_cond_init(&gate.changed) != 0) {
            sixel_mutex_destroy(&gate.mutex);
            goto end;
        }
        gate_ready = 1;
        hooks.context = &gate;
        hooks.async_launched = fhedt_build_launched;
        hooks.async_finished = fhedt_build_finished;
        encoder->palette_test_hooks = &hooks;
    }
    /* Explicit bounded sampling retains the asynchronous dependency. */
    if (SIXEL_FAILED(sixel_encoder_setopt(encoder, '=', "greedy:B8")) ||
            SIXEL_FAILED(sixel_encoder_setopt(encoder, 'G', "off")) ||
            SIXEL_FAILED(sixel_encoder_setopt(encoder, '~', "fhedt")) ||
            SIXEL_FAILED(sixel_encoder_setopt(encoder, 'p', "16")) ||
            SIXEL_FAILED(sixel_encoder_setopt(encoder, 'C', "64")) ||
            SIXEL_FAILED(sixel_encoder_setopt(encoder, '4',
                "adaptive-grid"))) {
        goto end;
    }
    if (quality &&
            (SIXEL_FAILED(sixel_encoder_setopt(encoder, 'Q', "kmeans")) ||
             SIXEL_FAILED(sixel_encoder_setopt(encoder, 'F', "ward")) ||
             SIXEL_FAILED(sixel_encoder_setopt(encoder, 'W', "cielab")) ||
             SIXEL_FAILED(sixel_encoder_setopt(encoder, 'X', "cielab")) ||
             SIXEL_FAILED(sixel_encoder_setopt(encoder, 'd', "lso2")))) {
        goto end;
    }
    if (SIXEL_FAILED(sixel_frame_init_borrowed(frame, pixels, size, size,
            SIXEL_PIXELFORMAT_RGB888, NULL, -1))) {
        goto end;
    }
    ok = SIXEL_SUCCEEDED(sixel_encoder_encode_frame(encoder, frame, output));
end:
    sixel_output_unref(output);
    sixel_frame_unref(frame);
    sixel_encoder_unref(encoder);
    if (gate_ready) {
        ok = ok && gate.observed;
        sixel_cond_destroy(&gate.changed);
        sixel_mutex_destroy(&gate.mutex);
    }
    free(pixels);
    return ok ? fhedt_build_trace(path, overlap) : EXIT_FAILURE;
}
