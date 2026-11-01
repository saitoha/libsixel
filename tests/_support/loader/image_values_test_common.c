/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2026 libsixel developers. See `AUTHORS`.
 * Copyright (c) 2014-2016 Hayaki Saito
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

/* Compare every decoded RGB sample with a separately prepared reference.
 * Native palette decoding remains enabled, as in the default loader path.
 * Shell callers own the fixture provenance and the per-channel error budget.
 * This observer does not resize images, pool errors or invoke a quality
 * metric. Matching dimensions and finite samples are mandatory. */
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "src/frame-private.h"
#include "src/loader.h"
#include "src/loader-common.h"

typedef struct image_values_state {
    float *pixels;
    size_t count;
    int width;
    int height;
    int calls;
    int selected;
    int background;
} image_values_state_t;

static SIXELSTATUS
image_values_capture(sixel_frame_t *frame, void *context)
{
    image_values_state_t *state;
    SIXELSTATUS status;
    size_t count;

    state = context;
    if (state->calls++ != state->selected) {
        return SIXEL_OK;
    }
    status = sixel_frame_strip_alpha(frame, NULL);
    if (SIXEL_SUCCEEDED(status)) {
        status = sixel_frame_set_pixelformat(frame,
            SIXEL_PIXELFORMAT_RGBFLOAT32);
    }
    if (SIXEL_FAILED(status)) {
        return status;
    }
    state->width = sixel_frame_get_width(frame);
    state->height = sixel_frame_get_height(frame);
    if (state->width <= 0 || state->height <= 0 ||
            (size_t)state->width > SIZE_MAX / (size_t)state->height / 3u /
                sizeof(float)) {
        return SIXEL_BAD_INPUT;
    }
    count = (size_t)state->width * (size_t)state->height * 3u;
    state->pixels = malloc(count * sizeof(float));
    if (state->pixels == NULL) {
        return SIXEL_BAD_ALLOCATION;
    }
    memcpy(state->pixels, sixel_frame_get_pixels(frame),
        count * sizeof(float));
    state->count = count;
    return SIXEL_OK;
}

static SIXELSTATUS
image_values_load(image_values_state_t *state, char const *path,
                  char const *order)
{
    sixel_loader_t *loader;
    SIXELSTATUS status;
    int loop_control;
    int background_policy;
    int alpha_policy;
    unsigned char bgcolor[3];

    loader = NULL;
    alpha_policy = loader_transparent_policy();
    loop_control = SIXEL_LOOP_DISABLE;
    status = sixel_loader_new(&loader, NULL);
    /* Observe one animation cycle even when the file requests infinite
     * playback. A missing selected frame must fail instead of hanging. */
    if (SIXEL_SUCCEEDED(status)) {
        status = sixel_loader_setopt(loader, SIXEL_LOADER_OPTION_LOOP_CONTROL,
            &loop_control);
    }
    if (SIXEL_SUCCEEDED(status) && state->background != 0) {
        /* Supply an explicit background to the loader so its own linear
         * alpha composition is observed before palette construction. */
        memset(bgcolor, state->background == 2 ? 255 : 0, sizeof(bgcolor));
        background_policy = SIXEL_BACKGROUND_POLICY_EXPLICIT_FIRST;
        status = sixel_loader_setopt(loader, SIXEL_LOADER_OPTION_BGCOLOR,
            bgcolor);
        if (SIXEL_SUCCEEDED(status)) {
            status = sixel_loader_setopt(loader,
                SIXEL_LOADER_OPTION_BACKGROUND_POLICY, &background_policy);
        }
    }
    if (SIXEL_SUCCEEDED(status)) {
        status = sixel_loader_setopt(loader, SIXEL_LOADER_OPTION_CONTEXT,
            state);
    }
    if (SIXEL_SUCCEEDED(status)) {
        status = sixel_loader_setopt(loader, SIXEL_LOADER_OPTION_LOADER_ORDER,
            order);
    }
    if (SIXEL_SUCCEEDED(status)) {
        if (state->background != 0) {
            sixel_helper_set_loader_transparent_policy(
                SIXEL_ALPHA_POLICY_COMPOSITE);
        }
        status = sixel_loader_load_file(loader, path, image_values_capture);
        if (state->background != 0) {
            sixel_helper_set_loader_transparent_policy(alpha_policy);
        }
    }
    sixel_loader_unref(loader);
    return status;
}

/* Arguments: reference, actual, actual loader order, absolute RGB-byte
 * tolerance, optional zero-based actual frame. The reference must already
 * contain the intended gamma-sRGB samples; its metadata is not reapplied. */
int
test_image_values(int argc, char **argv)
{
    image_values_state_t reference;
    image_values_state_t actual;
    SIXELSTATUS status;
    double tolerance;
    double actual_value;
    int display;
    double error;
    double maximum;
    double sum;
    char *end;
    size_t i;
    size_t peak;
    int ok;

    if (argc != 5 && argc != 6) {
        return EXIT_FAILURE;
    }
    display = strcmp(argv[0], "loader/image_display_values") == 0;
    tolerance = strtod(argv[4], &end);
    if (end == argv[4] || *end != '\0' || !isfinite(tolerance) ||
            tolerance < 0.0) {
        return EXIT_FAILURE;
    }
    memset(&reference, 0, sizeof(reference));
    memset(&actual, 0, sizeof(actual));
    actual.selected = argc == 6 ? atoi(argv[5]) : 0;
    actual.background = strcmp(argv[0], "loader/image_black_values") == 0
        ? 1 : strcmp(argv[0], "loader/image_white_values") == 0 ? 2 : 0;
    ok = 0;
    status = image_values_load(&reference, argv[1], "builtin:Enone!");
    if (SIXEL_SUCCEEDED(status)) {
        status = image_values_load(&actual, argv[2], argv[3]);
    }
    if (SIXEL_FAILED(status) || reference.pixels == NULL ||
            actual.pixels == NULL || reference.width != actual.width ||
            reference.height != actual.height) {
        fprintf(stderr, "pixel reference: load/dimension failure %x, "
            "%dx%d versus %dx%d\n", status, reference.width,
            reference.height, actual.width, actual.height);
        goto end;
    }
    maximum = 0.0;
    sum = 0.0;
    peak = 0u;
    for (i = 0u; i < reference.count; ++i) {
        if (!isfinite(reference.pixels[i]) || !isfinite(actual.pixels[i])) {
            fprintf(stderr, "pixel reference: nonfinite sample %zu\n", i);
            goto end;
        }
        actual_value = actual.pixels[i];
        /* Display references describe the clipped sRGB endpoint. Preserve
         * raw samples in image_values for precision and gamut contracts. */
        if (display) {
            actual_value = fmin(1.0, fmax(0.0, actual_value));
        }
        error = fabs(actual_value - reference.pixels[i]) * 255.0;
        sum += error * error;
        if (error > maximum) {
            maximum = error;
            peak = i;
        }
    }
    actual_value = actual.pixels[peak];
    if (display) {
        actual_value = fmin(1.0, fmax(0.0, actual_value));
    }
    fprintf(stderr, "PIXELVALUES|size=%dx%d|max=%.9g|rms=%.9g|"
        "sample=%zu|expected=%.9g|actual=%.9g|limit=%.9g\n",
        actual.width, actual.height, maximum, sqrt(sum / reference.count),
        peak, reference.pixels[peak] * 255.0, actual_value * 255.0,
        tolerance);
    ok = maximum <= tolerance;
end:
    free(reference.pixels);
    free(actual.pixels);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}

/* A gradient fixture must retain a nonzero RGB sample range after
 * encoding. This directly rejects black/white collapse at any image size.
 * Arguments: encoded image, minimum range in RGB byte levels. */
int
test_image_sample_range(int argc, char **argv)
{
    image_values_state_t actual;
    SIXELSTATUS status;
    double minimum;
    double maximum;
    double required;
    char *end;
    size_t i;
    int ok;

    if (argc != 3) {
        return EXIT_FAILURE;
    }
    required = strtod(argv[2], &end);
    if (end == argv[2] || *end != '\0' || !isfinite(required) ||
            required <= 0.0) {
        return EXIT_FAILURE;
    }
    memset(&actual, 0, sizeof(actual));
    ok = 0;
    status = image_values_load(&actual, argv[1], "builtin:Enone!");
    if (SIXEL_FAILED(status) || actual.pixels == NULL) {
        fprintf(stderr, "sample range: image load failed: %x\n", status);
        goto end;
    }
    minimum = maximum = actual.pixels[0];
    for (i = 0u; i < actual.count; ++i) {
        if (!isfinite(actual.pixels[i])) {
            goto end;
        }
        minimum = fmin(minimum, actual.pixels[i]);
        maximum = fmax(maximum, actual.pixels[i]);
    }
    fprintf(stderr, "RGB sample range: %.9g byte levels (minimum %.9g)\n",
        (maximum - minimum) * 255.0, required);
    ok = (maximum - minimum) * 255.0 >= required;
end:
    free(actual.pixels);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}

/* Check decoded canvas dimensions without resampling or score pooling.
 * Arguments: image, expected width, expected height. */
int
test_image_dimensions(int argc, char **argv)
{
    image_values_state_t actual;
    SIXELSTATUS status;
    int width;
    int height;
    int ok;

    if (argc != 4) {
        return EXIT_FAILURE;
    }
    width = atoi(argv[2]);
    height = atoi(argv[3]);
    if (width <= 0 || height <= 0) {
        return EXIT_FAILURE;
    }
    memset(&actual, 0, sizeof(actual));
    status = image_values_load(&actual, argv[1], "builtin:Enone!");
    ok = SIXEL_SUCCEEDED(status) && actual.pixels != NULL &&
        actual.width == width && actual.height == height;
    fprintf(stderr, "image dimensions: %dx%d, expected %dx%d\n",
        actual.width, actual.height, width, height);
    free(actual.pixels);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
