/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2025 libsixel developers. See `AUTHORS`.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * The LPIPS helper evaluates perceptual similarity between two images.
 * The program stitches three subsystems together:
 *
 *  +-------------------+   +-------------------+   +---------------------+
 *  | libsixel loaders  |-->| tensor formatting |-->| ONNX Runtime graph  |
 *  +-------------------+   +-------------------+   +---------------------+
 */

#include "config.h"

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
# include <io.h>
# include <windows.h>
#else
# include <unistd.h>
#endif

#if defined(__APPLE__)
# include <mach-o/dyld.h>
#endif

#include "onnxruntime_c_api.h"

#include <sixel.h>

#ifndef SIXEL_LPIPS_MODEL_DIR
# define SIXEL_LPIPS_MODEL_DIR ""
#endif

#if !defined(PATH_MAX)
# define PATH_MAX 4096
#endif

#if defined(_WIN32)
# define SIXEL_PATH_SEP '\\'
# define SIXEL_PATH_LIST_SEP ';'
#else
# define SIXEL_PATH_SEP '/'
# define SIXEL_PATH_LIST_SEP ':'
#endif

#define SIXEL_LOCAL_MODELS_SEG1 ".."
#define SIXEL_LOCAL_MODELS_SEG2 "models"
#define SIXEL_LOCAL_MODELS_SEG3 "lpips"

typedef struct image_f32 {
    int width;
    int height;
    float *nchw;
} image_f32_t;

typedef struct loader_capture {
    sixel_frame_t *frame;
} loader_capture_t;

static const OrtApi *g_api = NULL;

/* fatalf prints a formatted fatal message and terminates the process. */
static void
fatalf(char const *format, ...)
{
    va_list args;

    va_start(args, format);
#if defined(__clang__)
# pragma clang diagnostic push
# pragma clang diagnostic ignored "-Wformat-nonliteral"
#elif defined(__GNUC__)
# pragma GCC diagnostic push
# pragma GCC diagnostic ignored "-Wformat-nonliteral"
#endif
    vfprintf(stderr, format, args);
#if defined(__clang__)
# pragma clang diagnostic pop
#elif defined(__GNUC__)
# pragma GCC diagnostic pop
#endif
    va_end(args);

    exit(EXIT_FAILURE);
}

/* ort_check converts OrtStatus failures into fatal errors with a message. */
static void
ort_check(OrtStatus *status)
{
    char const *message;

    if (status == NULL) {
        return;
    }
    message = g_api->GetErrorMessage(status);
    fprintf(stderr, "ONNX Runtime error: %s\n",
            message != NULL ? message : "(null)");
    g_api->ReleaseStatus(status);
    exit(EXIT_FAILURE);
}

/* path_accessible returns non-zero when the file is readable by the process. */
static int
path_accessible(char const *path)
{
#if defined(_WIN32)
    int rc;

    rc = _access(path, 4);
    return rc == 0;
#else
    return access(path, R_OK) == 0;
#endif
}

/* join_path appends `leaf` to `dir`, inserting a separator when required. */
static int
join_path(char const *dir, char const *leaf, char *buffer, size_t size)
{
    size_t dir_len;
    size_t leaf_len;
    int need_sep;
    size_t total;

    dir_len = strlen(dir);
    leaf_len = strlen(leaf);
    need_sep = 0;
    if (leaf_len > 0 && (leaf[0] == '/' || leaf[0] == '\\')) {
        dir_len = 0;
    } else if (dir_len > 0 && dir[dir_len - 1] != SIXEL_PATH_SEP) {
        need_sep = 1;
    }
    total = dir_len + need_sep + leaf_len + 1u;
    if (total > size) {
        return -1;
    }
    if (dir_len > 0) {
        memcpy(buffer, dir, dir_len);
    }
    if (need_sep) {
        buffer[dir_len] = SIXEL_PATH_SEP;
        ++dir_len;
    }
    if (leaf_len > 0) {
        memcpy(buffer + dir_len, leaf, leaf_len);
        dir_len += leaf_len;
    }
    buffer[dir_len] = '\0';
    return 0;
}

#if !defined(_WIN32) && !defined(__APPLE__) && !defined(__linux__)
/* resolve_from_path_env searches the PATH components for `name`. */
static int
resolve_from_path_env(char const *name, char *buffer, size_t size)
{
    char const *env;
    char const *cursor;
    char const *separator;
    size_t chunk_len;

    env = getenv("PATH");
    if (env == NULL || *env == '\0') {
        return -1;
    }
    cursor = env;
    while (*cursor != '\0') {
        separator = strchr(cursor, SIXEL_PATH_LIST_SEP);
        if (separator == NULL) {
            chunk_len = strlen(cursor);
        } else {
            chunk_len = (size_t)(separator - cursor);
        }
        if (chunk_len >= size) {
            return -1;
        }
        memcpy(buffer, cursor, chunk_len);
        buffer[chunk_len] = '\0';
        if (join_path(buffer, name, buffer, size) != 0) {
            return -1;
        }
        if (path_accessible(buffer)) {
            return 0;
        }
        if (separator == NULL) {
            break;
        }
        cursor = separator + 1;
    }
    return -1;
}
#endif

/* resolve_executable_dir deduces the directory that contains the binary. */
static int
resolve_executable_dir(char const *argv0, char *buffer, size_t size)
{
    char candidate[PATH_MAX];
    size_t length;
    char *slash;
#if defined(_WIN32)
    DWORD written;
#elif defined(__APPLE__)
    uint32_t bufsize;
#elif defined(__linux__)
    ssize_t count;
#endif

    (void) argv0;
    candidate[0] = '\0';
#if defined(_WIN32)
    written = GetModuleFileNameA(NULL, candidate, (DWORD)sizeof(candidate));
    if (written == 0 || written >= sizeof(candidate)) {
        return -1;
    }
#elif defined(__APPLE__)
    bufsize = (uint32_t)sizeof(candidate);
    if (_NSGetExecutablePath(candidate, &bufsize) != 0) {
        return -1;
    }
#elif defined(__linux__)
    count = readlink("/proc/self/exe", candidate, sizeof(candidate) - 1u);
    if (count < 0 || count >= (ssize_t)sizeof(candidate)) {
        return -1;
    }
    candidate[count] = '\0';
#else
    if (argv0 == NULL) {
        return -1;
    }
    if (strchr(argv0, '/') != NULL || strchr(argv0, '\\') != NULL) {
        if (strlen(argv0) >= sizeof(candidate)) {
            return -1;
        }
        strcpy(candidate, argv0);
    } else if (resolve_from_path_env(argv0, candidate,
                                     sizeof(candidate)) != 0) {
        return -1;
    }
    {
        char *resolved;

        resolved = realpath(candidate, NULL);
        if (resolved == NULL) {
            return -1;
        }
        if (strlen(resolved) >= sizeof(candidate)) {
            free(resolved);
            return -1;
        }
        strcpy(candidate, resolved);
        free(resolved);
    }
#endif
#if defined(_WIN32)
    {
        char *resolved;

        resolved = _fullpath(NULL, candidate, 0u);
        if (resolved != NULL) {
            if (strlen(resolved) < sizeof(candidate)) {
                strcpy(candidate, resolved);
            }
            free(resolved);
        }
    }
#endif
    length = strlen(candidate);
    if (length == 0) {
        return -1;
    }
    slash = strrchr(candidate, '/');
#if defined(_WIN32)
    if (slash == NULL) {
        slash = strrchr(candidate, '\\');
    }
#endif
    if (slash == NULL) {
        return -1;
    }
    *slash = '\0';
    if (strlen(candidate) + 1u > size) {
        return -1;
    }
    strcpy(buffer, candidate);
    return 0;
}

/* build_local_model_path constructs ../models/lpips/<name> relative to binary. */
static int
build_local_model_path(char const *binary_dir,
                       char const *name,
                       char *buffer,
                       size_t size)
{
    char stage1[PATH_MAX];
    char stage2[PATH_MAX];

    if (join_path(binary_dir, SIXEL_LOCAL_MODELS_SEG1,
                  stage1, sizeof(stage1)) != 0) {
        return -1;
    }
    if (join_path(stage1, SIXEL_LOCAL_MODELS_SEG2,
                  stage2, sizeof(stage2)) != 0) {
        return -1;
    }
    if (join_path(stage2, SIXEL_LOCAL_MODELS_SEG3,
                  stage1, sizeof(stage1)) != 0) {
        return -1;
    }
    if (join_path(stage1, name, buffer, size) != 0) {
        return -1;
    }
    return 0;
}

/* find_model prefers the local models directory and falls back to installs. */
static int
find_model(char const *binary_dir,
           char const *name,
           char *buffer,
           size_t size)
{
    char stage1[PATH_MAX];
    char const *env_dir;

    env_dir = getenv("LIBSIXEL_MODEL_DIR");
    if (env_dir != NULL && env_dir[0] != '\0') {
        if (join_path(env_dir, SIXEL_LOCAL_MODELS_SEG3,
                      stage1, sizeof(stage1)) == 0) {
            if (join_path(stage1, name, buffer, size) == 0) {
                if (path_accessible(buffer)) {
                    return 0;
                }
            }
        }
    }
    if (SIXEL_LPIPS_MODEL_DIR[0] != '\0') {
        if (join_path(SIXEL_LPIPS_MODEL_DIR, SIXEL_LOCAL_MODELS_SEG3,
                      stage1, sizeof(stage1)) == 0) {
            if (join_path(stage1, name, buffer, size) == 0) {
                if (path_accessible(buffer)) {
                    return 0;
                }
            }
        }
    }
    if (build_local_model_path(binary_dir, name, buffer, size) == 0) {
        if (path_accessible(buffer)) {
            return 0;
        }
    }
    return -1;
}

/* capture_first_frame stores the first decoded frame from the loader pipeline. */
static SIXELSTATUS
capture_first_frame(sixel_frame_t *frame, void *context)
{
    loader_capture_t *capture;
    capture = (loader_capture_t *)context;
    if (capture->frame == NULL) {
        sixel_frame_ref(frame);
        capture->frame = frame;
    }
    return SIXEL_OK;
}

/* copy_frame_to_rgb normalizes frame pixels to RGB888 and copies them out. */
static SIXELSTATUS
copy_frame_to_rgb(sixel_frame_t *frame,
                  sixel_allocator_t *allocator,
                  unsigned char **pixels,
                  int *width,
                  int *height)
{
    SIXELSTATUS status;
    int frame_width;
    int frame_height;
    int pixelformat;
    size_t size;
    unsigned char *buffer;
    int normalized_format;

    (void)allocator;

    frame_width = sixel_frame_get_width(frame);
    frame_height = sixel_frame_get_height(frame);
    status = sixel_frame_strip_alpha(frame, NULL);
    if (SIXEL_FAILED(status)) {
        return status;
    }
    pixelformat = sixel_frame_get_pixelformat(frame);
    size = (size_t)frame_width * (size_t)frame_height * 3u;
    /*
     * Use malloc here because the loader's allocator enforces a strict
     * allocation ceiling that can reject large frames even on hosts with
     * sufficient RAM available.
     */
    buffer = (unsigned char *)malloc(size);
    if (buffer == NULL) {
        return SIXEL_BAD_ALLOCATION;
    }
    if (pixelformat == SIXEL_PIXELFORMAT_RGB888) {
        memcpy(buffer, sixel_frame_get_pixels(frame), size);
    } else {
        normalized_format = pixelformat;
        status = sixel_helper_normalize_pixelformat(
            buffer,
            &normalized_format,
            sixel_frame_get_pixels(frame),
            pixelformat,
            frame_width,
            frame_height);
        if (SIXEL_FAILED(status)) {
            free(buffer);
            return status;
        }
    }
    *pixels = buffer;
    *width = frame_width;
    *height = frame_height;
    return SIXEL_OK;
}

/* load_image_to_nchw converts an image file into normalized float tensors. */
static SIXELSTATUS
load_image_to_nchw(char const *path,
                   sixel_allocator_t *allocator,
                   image_f32_t *image)
{
    loader_capture_t capture = { NULL };
    SIXELSTATUS status;
    unsigned char *pixels;
    int width;
    int height;
    float *nchw;
    size_t plane_size;
    size_t index;
    int x;
    int y;

    capture.frame = NULL;
    pixels = NULL;
    image->width = 0;
    image->height = 0;
    image->nchw = NULL;
    status = sixel_helper_load_image_file(path,
                                          1,     /* force_static */
                                          0,     /* fuse_palette */
                                          SIXEL_PALETTE_MAX,
                                          NULL,
                                          SIXEL_LOOP_DISABLE,
                                          capture_first_frame,
                                          0,     /* finsecure */
                                          NULL,  /* cancel_flag */
                                          NULL,  /* loader_order */
                                          &capture,
                                          allocator);
    if (SIXEL_FAILED(status)) {
        return status;
    }
    status = copy_frame_to_rgb(capture.frame,
                               allocator,
                               &pixels,
                               &width,
                               &height);
    if (SIXEL_FAILED(status)) {
        sixel_frame_unref(capture.frame);
        return status;
    }
    nchw = (float *)malloc(
        (size_t)width * (size_t)height * 3u * sizeof(float));
    if (nchw == NULL) {
        free(pixels);
        sixel_frame_unref(capture.frame);
        return SIXEL_BAD_ALLOCATION;
    }
    plane_size = (size_t)width * (size_t)height;
    for (y = 0; y < height; ++y) {
        for (x = 0; x < width; ++x) {
            index = (size_t)y * (size_t)width + (size_t)x;
            nchw[plane_size * 0u + index] =
                pixels[index * 3u + 0u] / 127.5f - 1.0f;
            nchw[plane_size * 1u + index] =
                pixels[index * 3u + 1u] / 127.5f - 1.0f;
            nchw[plane_size * 2u + index] =
                pixels[index * 3u + 2u] / 127.5f - 1.0f;
        }
    }
    image->width = width;
    image->height = height;
    image->nchw = nchw;
    free(pixels);
    sixel_frame_unref(capture.frame);
    return SIXEL_OK;
}

/* free_image releases heap buffers associated with the image container. */
static void
free_image(image_f32_t *image)
{
    if (image->nchw != NULL) {
        free(image->nchw);
        image->nchw = NULL;
    }
}

/* bilinear_resize_nchw3 rescales a planar NCHW image with bilinear weights. */
static float *
bilinear_resize_nchw3(float const *src,
                      int src_width,
                      int src_height,
                      int dst_width,
                      int dst_height)
{
    float *dst;
    int channel;
    int y;
    int x;
    float scale_y;
    float scale_x;
    float fy;
    float fx;
    int y0;
    int x0;
    float wy;
    float wx;
    size_t src_stride;
    size_t dst_index;

    dst = (float *)malloc((size_t)3 * (size_t)dst_height *
                           (size_t)dst_width * sizeof(float));
    if (dst == NULL) {
        return NULL;
    }
    src_stride = (size_t)src_width * (size_t)src_height;
    for (channel = 0; channel < 3; ++channel) {
        for (y = 0; y < dst_height; ++y) {
            scale_y = (float)src_height / (float)dst_height;
            fy = (float)y * scale_y;
            y0 = (int)fy;
            if (y0 >= src_height - 1) {
                y0 = src_height - 2;
            }
            wy = fy - (float)y0;
            for (x = 0; x < dst_width; ++x) {
                scale_x = (float)src_width / (float)dst_width;
                fx = (float)x * scale_x;
                x0 = (int)fx;
                if (x0 >= src_width - 1) {
                    x0 = src_width - 2;
                }
                wx = fx - (float)x0;
                dst_index = (size_t)channel * (size_t)dst_width *
                            (size_t)dst_height +
                            (size_t)y * (size_t)dst_width + (size_t)x;
                dst[dst_index] =
                    (1.0f - wx) * (1.0f - wy) *
                        src[(size_t)channel * src_stride +
                            (size_t)y0 * (size_t)src_width + (size_t)x0] +
                    wx * (1.0f - wy) *
                        src[(size_t)channel * src_stride +
                            (size_t)y0 * (size_t)src_width +
                            (size_t)(x0 + 1)] +
                    (1.0f - wx) * wy *
                        src[(size_t)channel * src_stride +
                            (size_t)(y0 + 1) * (size_t)src_width +
                            (size_t)x0] +
                    wx * wy *
                        src[(size_t)channel * src_stride +
                            (size_t)(y0 + 1) * (size_t)src_width +
                            (size_t)(x0 + 1)];
            }
        }
    }
    return dst;
}

/* get_first_input_shape queries an ONNX session for its primary input tensor. */
static void
get_first_input_shape(OrtSession *session, int64_t *dims, size_t *rank)
{
    OrtTypeInfo *type_info;
    OrtTensorTypeAndShapeInfo const *shape_info;

    type_info = NULL;
    shape_info = NULL;
    ort_check(g_api->SessionGetInputTypeInfo(session, 0, &type_info));
    ort_check(g_api->CastTypeInfoToTensorInfo(type_info, &shape_info));
    ort_check(g_api->GetDimensionsCount(shape_info, rank));
    ort_check(g_api->GetDimensions(shape_info, dims, *rank));
    g_api->ReleaseTypeInfo(type_info);
}

/* tail_index extracts the trailing integer from tensor names like feat_x_3. */
static int
tail_index(char const *name)
{
    int length;
    int index;

    length = (int)strlen(name);
    index = length - 1;
    while (index >= 0 && isdigit((unsigned char)name[index])) {
        --index;
    }
    if (index == length - 1) {
        return -1;
    }
    return atoi(name + index + 1);
}

/* run_lpips drives both ONNX graphs to compute the perceptual distance. */
static void
run_lpips(char const *diff_model,
          char const *feat_model,
          image_f32_t *image_a,
          image_f32_t *image_b)
{
    OrtEnv *env;
    OrtAllocator *allocator;
    OrtSessionOptions *options;
    OrtSession *diff_session;
    OrtSession *feat_session;
    OrtMemoryInfo *memory_info;
    OrtValue *tensor_a;
    OrtValue *tensor_b;
    OrtValue **features_a;
    OrtValue **features_b;
    OrtValue const **diff_values;
    OrtValue *diff_outputs[1];
    char *feat_input_name;
    char **feat_output_names;
    char **diff_input_names;
    char *diff_output_name;
    int64_t feat_dims[8];
    size_t feat_rank;
    size_t feat_outputs;
    size_t diff_inputs;
    int target_width;
    int target_height;
    float *resized_a;
    float *resized_b;
    float const *tensor_data_a;
    float const *tensor_data_b;
    size_t plane_size;
    size_t i;
    int64_t tensor_shape[4];

    env = NULL;
    allocator = NULL;
    options = NULL;
    diff_session = NULL;
    feat_session = NULL;
    memory_info = NULL;
    tensor_a = NULL;
    tensor_b = NULL;
    features_a = NULL;
    features_b = NULL;
    diff_values = NULL;
    diff_output_name = NULL;
    feat_input_name = NULL;
    feat_output_names = NULL;
    diff_input_names = NULL;
    resized_a = NULL;
    resized_b = NULL;

    g_api = OrtGetApiBase()->GetApi(ORT_API_VERSION);
    ort_check(g_api->CreateEnv(ORT_LOGGING_LEVEL_WARNING,
                               "lpips",
                               &env));
    ort_check(g_api->GetAllocatorWithDefaultOptions(&allocator));
    ort_check(g_api->CreateSessionOptions(&options));
    ort_check(g_api->CreateSession(env, diff_model, options, &diff_session));
    ort_check(g_api->CreateSession(env, feat_model, options, &feat_session));

    get_first_input_shape(feat_session, feat_dims, &feat_rank);
    target_width = (feat_rank >= 4 && feat_dims[3] > 0)
        ? (int)feat_dims[3]
        : image_a->width;
    target_height = (feat_rank >= 4 && feat_dims[2] > 0)
        ? (int)feat_dims[2]
        : image_a->height;

    resized_a = NULL;
    resized_b = NULL;
    tensor_data_a = image_a->nchw;
    tensor_data_b = image_b->nchw;
    if (image_a->width != target_width ||
        image_a->height != target_height) {
        resized_a = bilinear_resize_nchw3(image_a->nchw,
                                          image_a->width,
                                          image_a->height,
                                          target_width,
                                          target_height);
        if (resized_a == NULL) {
            fatalf("Out of memory while resizing image A\n");
        }
        tensor_data_a = resized_a;
    }
    if (image_b->width != target_width ||
        image_b->height != target_height) {
        resized_b = bilinear_resize_nchw3(image_b->nchw,
                                          image_b->width,
                                          image_b->height,
                                          target_width,
                                          target_height);
        if (resized_b == NULL) {
            fatalf("Out of memory while resizing image B\n");
        }
        tensor_data_b = resized_b;
    }

    plane_size = (size_t)target_width * (size_t)target_height;
    tensor_shape[0] = 1;
    tensor_shape[1] = 3;
    tensor_shape[2] = target_height;
    tensor_shape[3] = target_width;

    ort_check(g_api->CreateCpuMemoryInfo(OrtArenaAllocator,
                                         OrtMemTypeDefault,
                                         &memory_info));
    ort_check(g_api->CreateTensorWithDataAsOrtValue(
        memory_info,
        (void *)tensor_data_a,
        plane_size * 3u * sizeof(float),
        tensor_shape,
        4,
        ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT,
        &tensor_a));
    ort_check(g_api->CreateTensorWithDataAsOrtValue(
        memory_info,
        (void *)tensor_data_b,
        plane_size * 3u * sizeof(float),
        tensor_shape,
        4,
        ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT,
        &tensor_b));

    ort_check(g_api->SessionGetInputName(feat_session,
                                         0,
                                         allocator,
                                         &feat_input_name));
    ort_check(g_api->SessionGetOutputCount(feat_session,
                                           &feat_outputs));
    feat_output_names = (char **)calloc(feat_outputs, sizeof(char *));
    features_a = (OrtValue **)calloc(feat_outputs, sizeof(OrtValue *));
    features_b = (OrtValue **)calloc(feat_outputs, sizeof(OrtValue *));
    if (feat_output_names == NULL ||
        features_a == NULL ||
        features_b == NULL) {
        fatalf("Out of memory while preparing feature arrays\n");
    }
    for (i = 0; i < feat_outputs; ++i) {
        ort_check(g_api->SessionGetOutputName(feat_session,
                                              i,
                                              allocator,
                                              &feat_output_names[i]));
    }
    ort_check(g_api->Run(feat_session,
                         NULL,
                         (char const *const *)&feat_input_name,
                         (OrtValue const *const *)&tensor_a,
                         1,
                         (char const *const *)feat_output_names,
                         feat_outputs,
                         features_a));
    ort_check(g_api->Run(feat_session,
                         NULL,
                         (char const *const *)&feat_input_name,
                         (OrtValue const *const *)&tensor_b,
                         1,
                         (char const *const *)feat_output_names,
                         feat_outputs,
                         features_b));

    ort_check(g_api->SessionGetInputCount(diff_session,
                                          &diff_inputs));
    diff_input_names = (char **)calloc(diff_inputs, sizeof(char *));
    diff_values = (OrtValue const **)calloc(diff_inputs,
                                            sizeof(OrtValue *));
    if (diff_input_names == NULL || diff_values == NULL) {
        fatalf("Out of memory while preparing diff inputs\n");
    }
    {
        int index;

        for (i = 0; i < diff_inputs; ++i) {
            ort_check(g_api->SessionGetInputName(diff_session,
                                                 i,
                                                 allocator,
                                                 &diff_input_names[i]));
            index = tail_index(diff_input_names[i]);
            if (strncmp(diff_input_names[i], "feat_x_", 7) == 0 &&
                index >= 0 && (size_t)index < feat_outputs) {
                diff_values[i] = features_a[index];
            } else if (strncmp(diff_input_names[i], "feat_y_", 7) == 0 &&
                       index >= 0 && (size_t)index < feat_outputs) {
                diff_values[i] = features_b[index];
            } else {
                fprintf(stderr,
                        "Warning: unmatched diff input: %s\n",
                        diff_input_names[i]);
                diff_values[i] = NULL;
            }
        }
    }

    ort_check(g_api->SessionGetOutputName(diff_session,
                                          0,
                                          allocator,
                                          &diff_output_name));
    diff_outputs[0] = NULL;
    ort_check(g_api->Run(diff_session,
                         NULL,
                         (char const *const *)diff_input_names,
                         diff_values,
                         diff_inputs,
                         (char const *const *)&diff_output_name,
                         1,
                         diff_outputs));
    {
        float *result_data;

        result_data = NULL;
        ort_check(g_api->GetTensorMutableData(diff_outputs[0],
                                              (void **)&result_data));
        printf("LPIPS: %.8f\n", result_data[0]);
    }

    if (diff_outputs[0] != NULL) {
        g_api->ReleaseValue(diff_outputs[0]);
    }
    if (diff_output_name != NULL) {
        ort_check(g_api->AllocatorFree(allocator, diff_output_name));
    }
    if (diff_input_names != NULL) {
        for (i = 0; i < diff_inputs; ++i) {
            if (diff_input_names[i] != NULL) {
                ort_check(g_api->AllocatorFree(allocator,
                                               diff_input_names[i]));
            }
        }
        free(diff_input_names);
    }
    if (diff_values != NULL) {
        free(diff_values);
    }
    if (feat_output_names != NULL) {
        for (i = 0; i < feat_outputs; ++i) {
            if (feat_output_names[i] != NULL) {
                ort_check(g_api->AllocatorFree(allocator,
                                               feat_output_names[i]));
            }
        }
        free(feat_output_names);
    }
    if (features_a != NULL) {
        for (i = 0; i < feat_outputs; ++i) {
            if (features_a[i] != NULL) {
                g_api->ReleaseValue(features_a[i]);
            }
        }
        free(features_a);
    }
    if (features_b != NULL) {
        for (i = 0; i < feat_outputs; ++i) {
            if (features_b[i] != NULL) {
                g_api->ReleaseValue(features_b[i]);
            }
        }
        free(features_b);
    }
    if (feat_input_name != NULL) {
        ort_check(g_api->AllocatorFree(allocator, feat_input_name));
    }
    if (tensor_a != NULL) {
        g_api->ReleaseValue(tensor_a);
    }
    if (tensor_b != NULL) {
        g_api->ReleaseValue(tensor_b);
    }
    if (memory_info != NULL) {
        g_api->ReleaseMemoryInfo(memory_info);
    }
    if (feat_session != NULL) {
        g_api->ReleaseSession(feat_session);
    }
    if (diff_session != NULL) {
        g_api->ReleaseSession(diff_session);
    }
    if (options != NULL) {
        g_api->ReleaseSessionOptions(options);
    }
    if (env != NULL) {
        g_api->ReleaseEnv(env);
    }
    if (resized_a != NULL) {
        free(resized_a);
    }
    if (resized_b != NULL) {
        free(resized_b);
    }
}

/* main wires argument parsing, model discovery, and image ingestion together. */
int
main(int argc, char **argv)
{
    sixel_allocator_t *allocator;
    image_f32_t image_a;
    image_f32_t image_b;
    SIXELSTATUS status;
    char binary_dir[PATH_MAX];
    char diff_model[PATH_MAX];
    char feat_model[PATH_MAX];

    allocator = NULL;
    image_a.width = 0;
    image_a.height = 0;
    image_a.nchw = NULL;
    image_b.width = 0;
    image_b.height = 0;
    image_b.nchw = NULL;

    if (argc != 3) {
        fprintf(stderr, "Usage: %s <imageA> <imageB>\n", argv[0]);
        return EXIT_FAILURE;
    }
    if (resolve_executable_dir(argv[0], binary_dir,
                               sizeof(binary_dir)) != 0) {
        fatalf("Unable to determine executable directory\n");
    }
    if (find_model(binary_dir, "lpips_diff.onnx",
                   diff_model, sizeof(diff_model)) != 0) {
        fatalf("Failed to locate lpips_diff.onnx\n");
    }
    if (find_model(binary_dir, "lpips_feature.onnx",
                   feat_model, sizeof(feat_model)) != 0) {
        fatalf("Failed to locate lpips_feature.onnx\n");
    }

    status = sixel_allocator_new(&allocator,
                                 malloc,
                                 calloc,
                                 realloc,
                                 free);
    if (SIXEL_FAILED(status)) {
        fatalf("Failed to allocate memory: %s\n",
               sixel_helper_format_error(status));
    }
    status = load_image_to_nchw(argv[1], allocator, &image_a);
    if (SIXEL_FAILED(status)) {
        char const *detail_a;

        detail_a = sixel_helper_get_additional_message();
        if (detail_a != NULL && detail_a[0] != '\0') {
            fatalf("Failed to load %s: %s (%s)\n",
                   argv[1],
                   sixel_helper_format_error(status),
                   detail_a);
        }
        fatalf("Failed to load %s: %s\n",
               argv[1], sixel_helper_format_error(status));
    }
    status = load_image_to_nchw(argv[2], allocator, &image_b);
    if (SIXEL_FAILED(status)) {
        char const *detail_b;

        detail_b = sixel_helper_get_additional_message();
        if (detail_b != NULL && detail_b[0] != '\0') {
            fatalf("Failed to load %s: %s (%s)\n",
                   argv[2],
                   sixel_helper_format_error(status),
                   detail_b);
        }
        fatalf("Failed to load %s: %s\n",
               argv[2], sixel_helper_format_error(status));
    }

    run_lpips(diff_model, feat_model, &image_a, &image_b);

    free_image(&image_a);
    free_image(&image_b);
    if (allocator != NULL) {
        sixel_allocator_unref(allocator);
    }

    return EXIT_SUCCESS;
}

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
