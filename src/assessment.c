/*
 * assessment.c - High-speed image quality evaluator ported from Python.
 *
 *  +-------------------------------------------------------------+
 *  |                        PIPELINE MAP                         |
 *  +---------------------+------------------+--------------------+
 *  | image loading (RGB) | metric kernels   | JSON emit          |
 *  |  (libsixel loader)  |  (MS-SSIM, etc.) | (stdout + files)   |
 *  +---------------------+------------------+--------------------+
 *
 *  ASCII flow for the metrics modules:
 *
 *        +-----------+      +---------------+      +-------------+
 *        |   Luma    | ---> |  Spectral +   | ---> |  Composite  |
 *        |  Stack    |      |  Spatial      |      |   Report    |
 *        +-----------+      +---------------+      +-------------+
 *              |                    |                      |
 *              |                    |                      +--> JSON writer
 *              |                    +--> FFT / histogram engines
 *              +--> MS-SSIM / PSNR / GMSD
 *
 *  Every function carries intentionally verbose comments so that future
 *  maintainers can follow the numerical steps without cross-referencing the
 *  removed Python original.
 */

#define _POSIX_C_SOURCE 200809L
#define _XOPEN_SOURCE 700

#include "config.h"

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <math.h>
#include <setjmp.h>
#include <stdbool.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sixel.h>

#include "assessment.h"
#include "frame.h"
#include "loader.h"

#if defined(_WIN32)
#include <io.h>
#include <windows.h>
#else
#include <unistd.h>
#endif

#if defined(__APPLE__)
#include <mach-o/dyld.h>
#endif

#if defined(__linux__)
#include <sys/stat.h>
#include <sys/types.h>
#endif

#if defined(HAVE_ONNXRUNTIME)
#include "onnxruntime_c_api.h"
#endif

#ifndef SIXEL_LPIPS_MODEL_DIR
#define SIXEL_LPIPS_MODEL_DIR ""
#endif

#if !defined(PATH_MAX)
#define PATH_MAX 4096
#endif

#if defined(_WIN32)
#define SIXEL_PATH_SEP '\\'
#define SIXEL_PATH_LIST_SEP ';'
#else
#define SIXEL_PATH_SEP '/'
#define SIXEL_PATH_LIST_SEP ':'
#endif

#define SIXEL_LOCAL_MODELS_SEG1 ".."
#define SIXEL_LOCAL_MODELS_SEG2 "models"
#define SIXEL_LOCAL_MODELS_SEG3 "lpips"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

typedef struct Image {
    int width;
    int height;
    int channels;
    float *pixels; /* interleaved RGB, values in [0,1] */
} Image;

typedef struct FloatBuffer {
    size_t length;
    float *values;
} FloatBuffer;

typedef struct Complex {
    double re;
    double im;
} Complex;

typedef struct Metrics {
    float ms_ssim;
    float high_freq_out;
    float high_freq_ref;
    float high_freq_delta;
    float stripe_ref;
    float stripe_out;
    float stripe_rel;
    float band_run_rel;
    float band_grad_rel;
    float clip_l_ref;
    float clip_r_ref;
    float clip_g_ref;
    float clip_b_ref;
    float clip_l_out;
    float clip_r_out;
    float clip_g_out;
    float clip_b_out;
    float clip_l_rel;
    float clip_r_rel;
    float clip_g_rel;
    float clip_b_rel;
    float delta_chroma_mean;
    float delta_e00_mean;
    float gmsd_value;
    float psnr_y;
    float lpips_vgg;
} Metrics;

typedef struct sixel_assessment_capture {
    sixel_frame_t *frame;
} sixel_assessment_capture_t;

static sixel_assessment_t *g_assessment_context = NULL;

static int assessment_resolve_executable_dir(char const *argv0,
                                            char *buffer,
                                            size_t size);
static void align_images(Image *ref, Image *out);

static void
assessment_fail(SIXELSTATUS status, char const *message)
{
    sixel_assessment_t *ctx;
    size_t length;

    ctx = g_assessment_context;
    if (ctx != NULL) {
        ctx->last_error = status;
        if (message != NULL) {
            length = strlen(message);
            if (length >= sizeof(ctx->error_message)) {
                length = sizeof(ctx->error_message) - 1u;
            }
            memcpy(ctx->error_message, message, length);
            ctx->error_message[length] = '\0';
        } else {
            ctx->error_message[0] = '\0';
        }
        longjmp(ctx->bailout, 1);
    }
    if (message != NULL) {
        fprintf(stderr, "%s\n", message);
    } else {
        fprintf(stderr, "assessment failure\n");
    }
    abort();
}

/*
 * Memory helpers
 */
static void *xmalloc(size_t size)
{
    void *ptr;
    ptr = malloc(size);
    if (ptr == NULL) {
        assessment_fail(SIXEL_BAD_ALLOCATION,
                       "malloc failed while building assessment state");
    }
    return ptr;
}

static void *xcalloc(size_t nmemb, size_t size)
{
    void *ptr;
    ptr = calloc(nmemb, size);
    if (ptr == NULL) {
        assessment_fail(SIXEL_BAD_ALLOCATION,
                       "calloc failed while building assessment state");
    }
    return ptr;
}

static void image_free(Image *img)
{
    if (img->pixels != NULL) {
        free(img->pixels);
        img->pixels = NULL;
    }
}

/*
 * Loader bridge (libsixel -> float RGB)
 */
static SIXELSTATUS copy_frame_to_rgb(sixel_frame_t *frame,
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

static SIXELSTATUS
image_from_frame(sixel_frame_t *frame, Image *img)
{
    SIXELSTATUS status;
    unsigned char *pixels;
    int width;
    int height;
    float *converted;
    size_t count;
    size_t index;

    pixels = NULL;
    width = 0;
    height = 0;
    converted = NULL;
    count = 0;
    index = 0;

    status = copy_frame_to_rgb(frame, &pixels, &width, &height);
    if (SIXEL_FAILED(status)) {
        return status;
    }
    count = (size_t)width * (size_t)height * 3u;
    converted = (float *)xmalloc(count * sizeof(float));
    for (index = 0; index < count; ++index) {
        converted[index] = pixels[index] / 255.0f;
    }
    img->width = width;
    img->height = height;
    img->channels = 3;
    img->pixels = converted;

    free(pixels);
    return SIXEL_OK;
}

/*
 * Path discovery helpers (shared by CLI + LPIPS bridge)
 */
#if defined(HAVE_ONNXRUNTIME) || \
    (!defined(_WIN32) && !defined(__APPLE__) && !defined(__linux__))
static int path_accessible(char const *path)
{
#if defined(_WIN32)
    int rc;

    rc = _access(path, 4);
    return rc == 0;
#else
    return access(path, R_OK) == 0;
#endif
}

static int
join_path(char const *dir,
          char const *leaf,
          char *buffer,
          size_t size)
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
#endif

#if !defined(_WIN32) && !defined(__APPLE__) && !defined(__linux__)
static int
resolve_from_path_env(char const *name,
                      char *buffer,
                      size_t size)
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

static int
assessment_resolve_executable_dir(char const *argv0,
                                  char *buffer,
                                  size_t size)
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

    candidate[0] = '\0';
#if defined(_WIN32) || defined(__APPLE__) || defined(__linux__)
    (void)argv0;
#endif
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

#if defined(HAVE_ONNXRUNTIME)
/*
 * LPIPS helper plumbing (model discovery + tensor formatting)
 */
typedef struct image_f32 {
    int width;
    int height;
    float *nchw;
} image_f32_t;

static const OrtApi *g_lpips_api = NULL;

static int build_local_model_path(char const *binary_dir,
                                  char const *name,
                                  char *buffer,
                                  size_t size)
{
    char stage1[PATH_MAX];
    char stage2[PATH_MAX];

    if (binary_dir == NULL || binary_dir[0] == '\0') {
        return -1;
    }
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

static int find_model(char const *binary_dir,
                      char const *override_dir,
                      char const *name,
                      char *buffer,
                      size_t size)
{
    char env_root[PATH_MAX];
    char install_root[PATH_MAX];
    char const *env_dir;

    env_dir = getenv("LIBSIXEL_MODEL_DIR");
    if (env_dir != NULL && env_dir[0] != '\0') {
        if (join_path(env_dir, SIXEL_LOCAL_MODELS_SEG3,
                      env_root, sizeof(env_root)) == 0) {
            if (join_path(env_root, name, buffer, size) == 0) {
                if (path_accessible(buffer)) {
                    return 0;
                }
            }
        }
    }
    if (override_dir != NULL && override_dir[0] != '\0') {
        if (join_path(override_dir, name, buffer, size) == 0) {
            if (path_accessible(buffer)) {
                return 0;
            }
        }
    }
    if (SIXEL_LPIPS_MODEL_DIR[0] != '\0') {
        if (join_path(SIXEL_LPIPS_MODEL_DIR, SIXEL_LOCAL_MODELS_SEG3,
                      install_root, sizeof(install_root)) == 0) {
            if (join_path(install_root, name, buffer, size) == 0) {
                if (path_accessible(buffer)) {
                    return 0;
                }
            }
        }
    }
    if (binary_dir != NULL && binary_dir[0] != '\0') {
        if (build_local_model_path(binary_dir, name, buffer, size) == 0) {
            if (path_accessible(buffer)) {
                return 0;
            }
        }
    }
    return -1;
}

static int
ensure_lpips_models(sixel_assessment_t *assessment)
{
    if (assessment->lpips_models_ready) {
        return 0;
    }
    if (find_model(assessment->binary_dir,
                   assessment->model_dir_state > 0
                       ? assessment->model_dir
                       : NULL,
                   "lpips_diff.onnx",
                   assessment->diff_model_path,
                   sizeof(assessment->diff_model_path)) != 0) {
        fprintf(stderr,
                "Warning: lpips_diff.onnx not found.\n");
        return -1;
    }
    if (find_model(assessment->binary_dir,
                   assessment->model_dir_state > 0
                       ? assessment->model_dir
                       : NULL,
                   "lpips_feature.onnx",
                   assessment->feat_model_path,
                   sizeof(assessment->feat_model_path)) != 0) {
        fprintf(stderr,
                "Warning: lpips_feature.onnx not found.\n");
        return -1;
    }
    assessment->lpips_models_ready = 1;
    return 0;
}

static void
free_image_f32(image_f32_t *image)
{
    if (image->nchw != NULL) {
        free(image->nchw);
        image->nchw = NULL;
    }
}

static int
convert_image_to_nchw(const Image *src, image_f32_t *dst)
{
    size_t plane_size;
    size_t index;
    float *buffer;

    if (src->channels != 3) {
        return -1;
    }
    plane_size = (size_t)src->width * (size_t)src->height;
    buffer = (float *)malloc(plane_size * 3u * sizeof(float));
    if (buffer == NULL) {
        return -1;
    }
    for (index = 0; index < plane_size; ++index) {
        buffer[plane_size * 0u + index] =
            src->pixels[index * 3u + 0u] * 2.0f - 1.0f;
        buffer[plane_size * 1u + index] =
            src->pixels[index * 3u + 1u] * 2.0f - 1.0f;
        buffer[plane_size * 2u + index] =
            src->pixels[index * 3u + 2u] * 2.0f - 1.0f;
    }
    dst->width = src->width;
    dst->height = src->height;
    dst->nchw = buffer;
    return 0;
}

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

static int
ort_status_to_error(OrtStatus *status)
{
    char const *message;

    if (status == NULL) {
        return 0;
    }
    message = g_lpips_api->GetErrorMessage(status);
    fprintf(stderr,
            "ONNX Runtime error: %s\n",
            message != NULL ? message : "(null)");
    g_lpips_api->ReleaseStatus(status);
    return -1;
}

static void
get_first_input_shape(OrtSession *session,
                      int64_t *dims,
                      size_t *rank)
{
    OrtTypeInfo *type_info;
    OrtTensorTypeAndShapeInfo const *shape_info;

    type_info = NULL;
    shape_info = NULL;
    if (ort_status_to_error(g_lpips_api->SessionGetInputTypeInfo(
            session, 0, &type_info)) != 0) {
        return;
    }
    if (ort_status_to_error(g_lpips_api->CastTypeInfoToTensorInfo(
            type_info, &shape_info)) != 0) {
        g_lpips_api->ReleaseTypeInfo(type_info);
        return;
    }
    if (ort_status_to_error(g_lpips_api->GetDimensionsCount(
            shape_info, rank)) != 0) {
        g_lpips_api->ReleaseTypeInfo(type_info);
        return;
    }
    (void)ort_status_to_error(g_lpips_api->GetDimensions(
        shape_info, dims, *rank));
    g_lpips_api->ReleaseTypeInfo(type_info);
}

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

static int
run_lpips(char const *diff_model,
          char const *feat_model,
          image_f32_t *image_a,
          image_f32_t *image_b,
          float *result_out)
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
    OrtStatus *status;
    int rc;

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
    diff_outputs[0] = NULL;
    feat_input_name = NULL;
    feat_output_names = NULL;
    diff_input_names = NULL;
    diff_output_name = NULL;
    target_width = image_a->width;
    target_height = image_a->height;
    resized_a = NULL;
    resized_b = NULL;
    tensor_data_a = image_a->nchw;
    tensor_data_b = image_b->nchw;
    feat_rank = 0;
    feat_outputs = 0;
    diff_inputs = 0;
    status = NULL;
    rc = -1;
    *result_out = NAN;

    g_lpips_api = OrtGetApiBase()->GetApi(ORT_API_VERSION);
    if (g_lpips_api == NULL) {
        fprintf(stderr, "ONNX Runtime API unavailable.\n");
        goto cleanup;
    }

    status = g_lpips_api->CreateEnv(ORT_LOGGING_LEVEL_WARNING,
                                    "lpips",
                                    &env);
    if (ort_status_to_error(status) != 0) {
        goto cleanup;
    }
    status = g_lpips_api->GetAllocatorWithDefaultOptions(&allocator);
    if (ort_status_to_error(status) != 0) {
        goto cleanup;
    }
    status = g_lpips_api->CreateSessionOptions(&options);
    if (ort_status_to_error(status) != 0) {
        goto cleanup;
    }
    status = g_lpips_api->CreateSession(env, diff_model, options,
                                        &diff_session);
    if (ort_status_to_error(status) != 0) {
        goto cleanup;
    }
    status = g_lpips_api->CreateSession(env, feat_model, options,
                                        &feat_session);
    if (ort_status_to_error(status) != 0) {
        goto cleanup;
    }

    get_first_input_shape(feat_session, feat_dims, &feat_rank);
    if (feat_rank >= 4 && feat_dims[3] > 0) {
        target_width = (int)feat_dims[3];
    }
    if (feat_rank >= 4 && feat_dims[2] > 0) {
        target_height = (int)feat_dims[2];
    }

    if (image_a->width != target_width ||
        image_a->height != target_height) {
        resized_a = bilinear_resize_nchw3(image_a->nchw,
                                          image_a->width,
                                          image_a->height,
                                          target_width,
                                          target_height);
        if (resized_a == NULL) {
            fprintf(stderr,
                    "Warning: unable to resize LPIPS reference tensor.\n");
            goto cleanup;
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
            fprintf(stderr,
                    "Warning: unable to resize LPIPS output tensor.\n");
            goto cleanup;
        }
        tensor_data_b = resized_b;
    }

    plane_size = (size_t)target_width * (size_t)target_height;
    tensor_shape[0] = 1;
    tensor_shape[1] = 3;
    tensor_shape[2] = target_height;
    tensor_shape[3] = target_width;

    status = g_lpips_api->CreateCpuMemoryInfo(OrtArenaAllocator,
                                              OrtMemTypeDefault,
                                              &memory_info);
    if (ort_status_to_error(status) != 0) {
        goto cleanup;
    }
    status = g_lpips_api->CreateTensorWithDataAsOrtValue(
        memory_info,
        (void *)tensor_data_a,
        plane_size * 3u * sizeof(float),
        tensor_shape,
        4,
        ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT,
        &tensor_a);
    if (ort_status_to_error(status) != 0) {
        goto cleanup;
    }
    status = g_lpips_api->CreateTensorWithDataAsOrtValue(
        memory_info,
        (void *)tensor_data_b,
        plane_size * 3u * sizeof(float),
        tensor_shape,
        4,
        ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT,
        &tensor_b);
    if (ort_status_to_error(status) != 0) {
        goto cleanup;
    }

    status = g_lpips_api->SessionGetInputName(feat_session,
                                              0,
                                              allocator,
                                              &feat_input_name);
    if (ort_status_to_error(status) != 0) {
        goto cleanup;
    }
    status = g_lpips_api->SessionGetOutputCount(feat_session,
                                                &feat_outputs);
    if (ort_status_to_error(status) != 0) {
        goto cleanup;
    }
    feat_output_names = (char **)calloc(feat_outputs, sizeof(char *));
    features_a = (OrtValue **)calloc(feat_outputs, sizeof(OrtValue *));
    features_b = (OrtValue **)calloc(feat_outputs, sizeof(OrtValue *));
    if (feat_output_names == NULL ||
        features_a == NULL ||
        features_b == NULL) {
        fprintf(stderr,
                "Warning: out of memory while preparing LPIPS features.\n");
        goto cleanup;
    }
    for (i = 0; i < feat_outputs; ++i) {
        status = g_lpips_api->SessionGetOutputName(feat_session,
                                                   i,
                                                   allocator,
                                                   &feat_output_names[i]);
        if (ort_status_to_error(status) != 0) {
            goto cleanup;
        }
    }
    status = g_lpips_api->Run(feat_session,
                              NULL,
                              (char const *const *)&feat_input_name,
                              (OrtValue const *const *)&tensor_a,
                              1,
                              (char const *const *)feat_output_names,
                              feat_outputs,
                              features_a);
    if (ort_status_to_error(status) != 0) {
        goto cleanup;
    }
    status = g_lpips_api->Run(feat_session,
                              NULL,
                              (char const *const *)&feat_input_name,
                              (OrtValue const *const *)&tensor_b,
                              1,
                              (char const *const *)feat_output_names,
                              feat_outputs,
                              features_b);
    if (ort_status_to_error(status) != 0) {
        goto cleanup;
    }

    status = g_lpips_api->SessionGetInputCount(diff_session,
                                               &diff_inputs);
    if (ort_status_to_error(status) != 0) {
        goto cleanup;
    }
    diff_input_names = (char **)calloc(diff_inputs, sizeof(char *));
    diff_values = (OrtValue const **)calloc(diff_inputs,
                                            sizeof(OrtValue *));
    if (diff_input_names == NULL || diff_values == NULL) {
        fprintf(stderr,
                "Warning: out of memory while preparing LPIPS diff inputs.\n");
        goto cleanup;
    }
    for (i = 0; i < diff_inputs; ++i) {
        status = g_lpips_api->SessionGetInputName(diff_session,
                                                  i,
                                                  allocator,
                                                  &diff_input_names[i]);
        if (ort_status_to_error(status) != 0) {
            goto cleanup;
        }
        if (diff_input_names[i] == NULL) {
            continue;
        }
        if (strncmp(diff_input_names[i], "feat_x_", 7) == 0) {
            int index;

            index = tail_index(diff_input_names[i]);
            if (index >= 0 && (size_t)index < feat_outputs) {
                diff_values[i] = features_a[index];
            }
        } else if (strncmp(diff_input_names[i], "feat_y_", 7) == 0) {
            int index;

            index = tail_index(diff_input_names[i]);
            if (index >= 0 && (size_t)index < feat_outputs) {
                diff_values[i] = features_b[index];
            }
        }
    }

    status = g_lpips_api->SessionGetOutputName(diff_session,
                                               0,
                                               allocator,
                                               &diff_output_name);
    if (ort_status_to_error(status) != 0) {
        goto cleanup;
    }
    status = g_lpips_api->Run(diff_session,
                              NULL,
                              (char const *const *)diff_input_names,
                              diff_values,
                              diff_inputs,
                              (char const *const *)&diff_output_name,
                              1,
                              diff_outputs);
    if (ort_status_to_error(status) != 0) {
        goto cleanup;
    }

    if (diff_outputs[0] != NULL) {
        float *result_data;

        result_data = NULL;
        status = g_lpips_api->GetTensorMutableData(diff_outputs[0],
                                                   (void **)&result_data);
        if (ort_status_to_error(status) != 0) {
            goto cleanup;
        }
        if (result_data != NULL) {
            *result_out = result_data[0];
            rc = 0;
        }
    }

cleanup:
    if (diff_outputs[0] != NULL) {
        g_lpips_api->ReleaseValue(diff_outputs[0]);
    }
    /*
     * Clean up ORT-managed string buffers with explicit status release.
     *
     * We always release the temporary OrtStatus objects to prevent
     * resource leaks when ONNX Runtime reports cleanup diagnostics.
     */
    if (diff_output_name != NULL) {
        status = g_lpips_api->AllocatorFree(allocator, diff_output_name);
        if (status != NULL) {
            g_lpips_api->ReleaseStatus(status);
        }
    }
    if (diff_input_names != NULL) {
        for (i = 0; i < diff_inputs; ++i) {
            if (diff_input_names[i] != NULL) {
                status = g_lpips_api->AllocatorFree(allocator,
                                                    diff_input_names[i]);
                if (status != NULL) {
                    g_lpips_api->ReleaseStatus(status);
                }
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
                status = g_lpips_api->AllocatorFree(allocator,
                                                    feat_output_names[i]);
                if (status != NULL) {
                    g_lpips_api->ReleaseStatus(status);
                }
            }
        }
        free(feat_output_names);
    }
    if (features_a != NULL) {
        for (i = 0; i < feat_outputs; ++i) {
            if (features_a[i] != NULL) {
                g_lpips_api->ReleaseValue(features_a[i]);
            }
        }
        free(features_a);
    }
    if (features_b != NULL) {
        for (i = 0; i < feat_outputs; ++i) {
            if (features_b[i] != NULL) {
                g_lpips_api->ReleaseValue(features_b[i]);
            }
        }
        free(features_b);
    }
    if (feat_input_name != NULL) {
        status = g_lpips_api->AllocatorFree(allocator, feat_input_name);
        if (status != NULL) {
            g_lpips_api->ReleaseStatus(status);
        }
    }
    if (tensor_a != NULL) {
        g_lpips_api->ReleaseValue(tensor_a);
    }
    if (tensor_b != NULL) {
        g_lpips_api->ReleaseValue(tensor_b);
    }
    if (memory_info != NULL) {
        g_lpips_api->ReleaseMemoryInfo(memory_info);
    }
    if (feat_session != NULL) {
        g_lpips_api->ReleaseSession(feat_session);
    }
    if (diff_session != NULL) {
        g_lpips_api->ReleaseSession(diff_session);
    }
    if (options != NULL) {
        g_lpips_api->ReleaseSessionOptions(options);
    }
    if (env != NULL) {
        g_lpips_api->ReleaseEnv(env);
    }
    if (resized_a != NULL) {
        free(resized_a);
    }
    if (resized_b != NULL) {
        free(resized_b);
    }
    return rc;
}
#endif /* HAVE_ONNXRUNTIME */

/*
 * Array math helpers
 */
static FloatBuffer
float_buffer_create(size_t length)
{
    FloatBuffer buf;
    buf.length = length;
    buf.values = (float *)xcalloc(length, sizeof(float));
    return buf;
}

static void
float_buffer_free(FloatBuffer *buf)
{
    if (buf->values != NULL) {
        free(buf->values);
        buf->values = NULL;
        buf->length = 0;
    }
}

static float
clamp_float(float v, float min_v, float max_v)
{
    float result;
    result = v;
    if (result < min_v) {
        result = min_v;
    }
    if (result > max_v) {
        result = max_v;
    }
    return result;
}

/*
 * Luma conversion and resizing utilities
 */
static FloatBuffer
image_to_luma709(const Image *img)
{
    FloatBuffer buf;
    size_t total;
    size_t i;
    const float *src;
    float r;
    float g;
    float b;

    total = (size_t)img->width * (size_t)img->height;
    buf = float_buffer_create(total);
    src = img->pixels;
    for (i = 0; i < total; ++i) {
        r = src[i * 3 + 0];
        g = src[i * 3 + 1];
        b = src[i * 3 + 2];
        buf.values[i] = 0.2126f * r + 0.7152f * g + 0.0722f * b;
    }
    return buf;
}

static FloatBuffer
image_channel(const Image *img, int channel)
{
    FloatBuffer buf;
    size_t total;
    size_t i;
    const float *src;
    float value;

    total = (size_t)img->width * (size_t)img->height;
    buf = float_buffer_create(total);
    src = img->pixels;
    for (i = 0; i < total; ++i) {
        value = src[i * 3 + channel];
        buf.values[i] = value;
    }
    return buf;
}
/*
 * Gaussian kernel and separable convolution
 */
static FloatBuffer gaussian_kernel1d(int size, float sigma)
{
    FloatBuffer kernel;
    int i;
    float mean;
    float sum;
    float x;
    float value;

    kernel = float_buffer_create((size_t)size);
    mean = ((float)size - 1.0f) * 0.5f;
    sum = 0.0f;
    for (i = 0; i < size; ++i) {
        x = (float)i - mean;
        value = expf(-0.5f * (x / sigma) * (x / sigma));
        kernel.values[i] = value;
        sum += value;
    }
    if (sum > 0.0f) {
        for (i = 0; i < size; ++i) {
            kernel.values[i] /= sum;
        }
    }
    return kernel;
}

static FloatBuffer
separable_conv2d(const FloatBuffer *img, int width,
                 int height, const FloatBuffer *kernel)
{
    FloatBuffer tmp;
    FloatBuffer out;
    int pad;
    int x;
    int y;
    int k;
    int kernel_size;
    float acc;
    int px;
    int py;
    int idx;
    float sample;

    pad = (int)kernel->length / 2;
    kernel_size = (int)kernel->length;
    tmp = float_buffer_create((size_t)width * (size_t)height);
    out = float_buffer_create((size_t)width * (size_t)height);

    for (y = 0; y < height; ++y) {
        for (x = 0; x < width; ++x) {
            acc = 0.0f;
            for (k = 0; k < kernel_size; ++k) {
                px = x + k - pad;
                if (px < 0) {
                    px = -px;
                }
                if (px >= width) {
                    px = width - (px - width) - 1;
                    if (px < 0) {
                        px = 0;
                    }
                }
                idx = y * width + px;
                sample = img->values[idx];
                acc += kernel->values[k] * sample;
            }
            tmp.values[y * width + x] = acc;
        }
    }

    for (y = 0; y < height; ++y) {
        for (x = 0; x < width; ++x) {
            acc = 0.0f;
            for (k = 0; k < kernel_size; ++k) {
                py = y + k - pad;
                if (py < 0) {
                    py = -py;
                }
                if (py >= height) {
                    py = height - (py - height) - 1;
                    if (py < 0) {
                        py = 0;
                    }
                }
                idx = py * width + x;
                sample = tmp.values[idx];
                acc += kernel->values[k] * sample;
            }
            out.values[y * width + x] = acc;
        }
    }

    float_buffer_free(&tmp);
    return out;
}

/*
 * SSIM and MS-SSIM computation
 */
static float ssim_luma(const FloatBuffer *x, const FloatBuffer *y,
                       int width, int height, float k1, float k2,
                       int win_size, float sigma)
{
    FloatBuffer kernel;
    FloatBuffer mu_x;
    FloatBuffer mu_y;
    FloatBuffer mu_x2;
    FloatBuffer mu_y2;
    FloatBuffer mu_xy;
    FloatBuffer sigma_x2;
    FloatBuffer sigma_y2;
    FloatBuffer sigma_xy;
    float C1;
    float C2;
    size_t total;
    size_t i;
    float mean;
    float numerator;
    float denominator;
    float value;
    FloatBuffer x_sq;
    FloatBuffer y_sq;
    FloatBuffer xy_buf;

    kernel = gaussian_kernel1d(win_size, sigma);
    mu_x = separable_conv2d(x, width, height, &kernel);
    mu_y = separable_conv2d(y, width, height, &kernel);

    total = (size_t)width * (size_t)height;
    mu_x2 = float_buffer_create(total);
    mu_y2 = float_buffer_create(total);
    mu_xy = float_buffer_create(total);
    sigma_x2 = float_buffer_create(total);
    sigma_y2 = float_buffer_create(total);
    sigma_xy = float_buffer_create(total);
    x_sq = float_buffer_create(total);
    y_sq = float_buffer_create(total);
    xy_buf = float_buffer_create(total);

    for (i = 0; i < total; ++i) {
        mu_x2.values[i] = mu_x.values[i] * mu_x.values[i];
        mu_y2.values[i] = mu_y.values[i] * mu_y.values[i];
        mu_xy.values[i] = mu_x.values[i] * mu_y.values[i];
        x_sq.values[i] = x->values[i] * x->values[i];
        y_sq.values[i] = y->values[i] * y->values[i];
        xy_buf.values[i] = x->values[i] * y->values[i];
    }

    float_buffer_free(&sigma_x2);
    float_buffer_free(&sigma_y2);
    float_buffer_free(&sigma_xy);
    sigma_x2 = separable_conv2d(&x_sq, width, height, &kernel);
    sigma_y2 = separable_conv2d(&y_sq, width, height, &kernel);
    sigma_xy = separable_conv2d(&xy_buf, width, height, &kernel);

    for (i = 0; i < total; ++i) {
        sigma_x2.values[i] -= mu_x2.values[i];
        sigma_y2.values[i] -= mu_y2.values[i];
        sigma_xy.values[i] -= mu_xy.values[i];
    }

    C1 = (k1 * 1.0f) * (k1 * 1.0f);
    C2 = (k2 * 1.0f) * (k2 * 1.0f);

    mean = 0.0f;
    for (i = 0; i < total; ++i) {
        numerator = (2.0f * mu_xy.values[i] + C1) *
                    (2.0f * sigma_xy.values[i] + C2);
        denominator = (mu_x2.values[i] + mu_y2.values[i] + C1) *
                      (sigma_x2.values[i] + sigma_y2.values[i] + C2);
        if (denominator != 0.0f) {
            value = numerator / (denominator + 1e-12f);
        } else {
            value = 0.0f;
        }
        mean += value;
    }
    mean /= (float)total;
    mean = clamp_float(mean, 0.0f, 1.0f);

    float_buffer_free(&kernel);
    float_buffer_free(&mu_x);
    float_buffer_free(&mu_y);
    float_buffer_free(&mu_x2);
    float_buffer_free(&mu_y2);
    float_buffer_free(&mu_xy);
    float_buffer_free(&sigma_x2);
    float_buffer_free(&sigma_y2);
    float_buffer_free(&sigma_xy);
    float_buffer_free(&x_sq);
    float_buffer_free(&y_sq);
    float_buffer_free(&xy_buf);

    return mean;
}

static FloatBuffer
downsample2(const FloatBuffer *img, int width, int height,
            int *new_width, int *new_height)
{
    int h2;
    int w2;
    int y;
    int x;
    FloatBuffer out;
    float sum;
    int idx0;
    int idx1;
    int idx2;
    int idx3;

    h2 = height / 2;
    w2 = width / 2;
    out = float_buffer_create((size_t)h2 * (size_t)w2);
    for (y = 0; y < h2; ++y) {
        for (x = 0; x < w2; ++x) {
            sum = 0.0f;
            idx0 = (2 * y) * width + (2 * x);
            idx1 = (2 * y + 1) * width + (2 * x);
            idx2 = (2 * y) * width + (2 * x + 1);
            idx3 = (2 * y + 1) * width + (2 * x + 1);
            sum += img->values[idx0];
            sum += img->values[idx1];
            sum += img->values[idx2];
            sum += img->values[idx3];
            out.values[y * w2 + x] = sum * 0.25f;
        }
    }
    *new_width = w2;
    *new_height = h2;
    return out;
}

static float
ms_ssim_luma(const FloatBuffer *ref, const FloatBuffer *out,
             int width, int height)
{
    static const float weights[5] = {
        0.0448f, 0.2856f, 0.3001f, 0.2363f, 0.1333f
    };
    FloatBuffer cur_ref;
    FloatBuffer cur_out;
    FloatBuffer next_ref;
    FloatBuffer next_out;
    int cur_width;
    int cur_height;
    float weighted_sum;
    float weight_total;
    int level;
    float ssim_value;
    int next_width;
    int next_height;

    cur_ref = float_buffer_create((size_t)width * (size_t)height);
    cur_out = float_buffer_create((size_t)width * (size_t)height);
    memcpy(cur_ref.values, ref->values,
           sizeof(float) * (size_t)width * (size_t)height);
    memcpy(cur_out.values, out->values,
           sizeof(float) * (size_t)width * (size_t)height);
    cur_width = width;
    cur_height = height;
    weighted_sum = 0.0f;
    weight_total = 0.0f;

    for (level = 0; level < 5; ++level) {
        ssim_value = ssim_luma(&cur_ref, &cur_out, cur_width, cur_height,
                               0.01f, 0.03f, 11, 1.5f);
        weighted_sum += ssim_value * weights[level];
        weight_total += weights[level];
        if (level < 4) {
            next_ref = downsample2(&cur_ref, cur_width, cur_height,
                                   &next_width, &next_height);
            next_out = downsample2(&cur_out, cur_width, cur_height,
                                   &next_width, &next_height);
            float_buffer_free(&cur_ref);
            float_buffer_free(&cur_out);
            cur_ref = next_ref;
            cur_out = next_out;
            cur_width = next_width;
            cur_height = next_height;
        }
    }

    float_buffer_free(&cur_ref);
    float_buffer_free(&cur_out);

    if (weight_total > 0.0f) {
        return weighted_sum / weight_total;
    }
    return 0.0f;
}
/*
 * FFT helpers for spectral metrics
 */
static int
next_power_of_two(int value)
{
    int n;
    n = 1;
    while (n < value) {
        n <<= 1;
    }
    return n;
}

static void
fft_bit_reverse(Complex *data, int n)
{
    int i;
    int j;
    int bit;
    Complex tmp;

    j = 0;
    for (i = 0; i < n; ++i) {
        if (i < j) {
            tmp = data[i];
            data[i] = data[j];
            data[j] = tmp;
        }
        bit = n >> 1;
        while ((j & bit) != 0) {
            j &= ~bit;
            bit >>= 1;
        }
        j |= bit;
    }
}

static void
fft_cooley_tukey(Complex *data, int n, int inverse)
{
    int len;
    double angle;
    Complex wlen;
    int half;
    int i;
    Complex w;
    int j;
    Complex u;
    Complex v;
    double tmp_re;
    double tmp_im;

    fft_bit_reverse(data, n);
    for (len = 2; len <= n; len <<= 1) {

        angle = 2.0 * M_PI / (double)len;
        if (inverse) {
            angle = -angle;
        }
        wlen.re = cos(angle);
        wlen.im = sin(angle);
        half = len >> 1;
        for (i = 0; i < n; i += len) {
            w.re = 1.0;
            w.im = 0.0;
            for (j = 0; j < half; ++j) {
                u = data[i + j];
                v.re = data[i + j + half].re * w.re -
                       data[i + j + half].im * w.im;
                v.im = data[i + j + half].re * w.im +
                       data[i + j + half].im * w.re;
                data[i + j].re = u.re + v.re;
                data[i + j].im = u.im + v.im;
                data[i + j + half].re = u.re - v.re;
                data[i + j + half].im = u.im - v.im;
                tmp_re = w.re * wlen.re - w.im * wlen.im;
                tmp_im = w.re * wlen.im + w.im * wlen.re;
                w.re = tmp_re;
                w.im = tmp_im;
            }
        }
    }
    if (inverse) {
        for (i = 0; i < n; ++i) {
            data[i].re /= n;
            data[i].im /= n;
        }
    }
}

static void
fft2d(FloatBuffer *input, int width, int height,
      Complex *output, int out_width, int out_height)
{
    int padded_width;
    int padded_height;
    int y;
    int x;
    Complex *row;
    Complex *col;
    Complex value;

    padded_width = out_width;
    padded_height = out_height;
    row = (Complex *)xmalloc(sizeof(Complex) * (size_t)padded_width);
    col = (Complex *)xmalloc(sizeof(Complex) * (size_t)padded_height);

    for (y = 0; y < padded_height; ++y) {
        for (x = 0; x < padded_width; ++x) {
            if (y < height && x < width) {
                value.re = input->values[y * width + x];
                value.im = 0.0;
            } else {
                value.re = 0.0;
                value.im = 0.0;
            }
            output[y * padded_width + x] = value;
        }
    }

    for (y = 0; y < padded_height; ++y) {
        for (x = 0; x < padded_width; ++x) {
            row[x] = output[y * padded_width + x];
        }
        fft_cooley_tukey(row, padded_width, 0);
        for (x = 0; x < padded_width; ++x) {
            output[y * padded_width + x] = row[x];
        }
    }

    for (x = 0; x < padded_width; ++x) {
        for (y = 0; y < padded_height; ++y) {
            col[y] = output[y * padded_width + x];
        }
        fft_cooley_tukey(col, padded_height, 0);
        for (y = 0; y < padded_height; ++y) {
            output[y * padded_width + x] = col[y];
        }
    }

    free(row);
    free(col);
}

static void
fft_shift(Complex *data, int width, int height)
{
    int half_w;
    int half_h;
    int y;
    int x;
    int nx;
    int ny;
    Complex tmp;

    half_w = width / 2;
    half_h = height / 2;
    for (y = 0; y < height; ++y) {
        for (x = 0; x < width; ++x) {
            nx = (x + half_w) % width;
            ny = (y + half_h) % height;
            if (ny > y || (ny == y && nx > x)) {
                continue;
            }
            tmp = data[y * width + x];
            data[y * width + x] = data[ny * width + nx];
            data[ny * width + nx] = tmp;
        }
    }
}
static float
high_frequency_ratio(const FloatBuffer *img,
                     int width, int height, float cutoff)
{
    int padded_width;
    int padded_height;
    Complex *freq;
    size_t total;
    FloatBuffer centered;
    double hi_sum;
    double total_sum;
    int y;
    int x;
    double cy;
    double cx;
    double mean;
    size_t i;
    double dy;
    double dx;
    double r;
    double norm;
    double power;

    padded_width = next_power_of_two(width);
    padded_height = next_power_of_two(height);
    total = (size_t)padded_width * (size_t)padded_height;
    freq = (Complex *)xmalloc(total * sizeof(Complex));
    centered = float_buffer_create((size_t)width * (size_t)height);

    for (y = 0; y < height; ++y) {
        for (x = 0; x < width; ++x) {
            centered.values[y * width + x] = img->values[y * width + x];
        }
    }
    mean = 0.0;
    for (i = 0; i < (size_t)width * (size_t)height; ++i) {
        mean += centered.values[i];
    }
    mean /= (double)((size_t)width * (size_t)height);
    for (i = 0; i < (size_t)width * (size_t)height; ++i) {
        centered.values[i] -= (float)mean;
    }

    fft2d(&centered, width, height, freq, padded_width, padded_height);
    fft_shift(freq, padded_width, padded_height);

    hi_sum = 0.0;
    total_sum = 0.0;
    cy = padded_height / 2.0;
    cx = padded_width / 2.0;

    for (y = 0; y < padded_height; ++y) {
        for (x = 0; x < padded_width; ++x) {
            dy = (double)y - cy;
            dx = (double)x - cx;
            r = sqrt(dy * dy + dx * dx);
            norm = r / (0.5 * sqrt((double)padded_height *
                                   (double)padded_height +
                                   (double)padded_width *
                                   (double)padded_width));
            power = freq[y * padded_width + x].re *
                    freq[y * padded_width + x].re +
                    freq[y * padded_width + x].im *
                    freq[y * padded_width + x].im;
            total_sum += power;
            if (norm >= cutoff) {
                hi_sum += power;
            }
        }
    }

    free(freq);
    float_buffer_free(&centered);

    if (total_sum <= 0.0) {
        return 0.0f;
    }
    return (float)(hi_sum / total_sum);
}

static float
stripe_score(const FloatBuffer *img, int width, int height,
             int bins)
{
    int padded_width;
    int padded_height;
    Complex *freq;
    FloatBuffer centered;
    double cy;
    double cx;
    double rmin;
    double *hist;
    int y;
    int x;
    double mean_val;
    double max_val;
    double mean;
    size_t i;
    double dy;
    double dx;
    double r;
    double ang;
    int index;
    double power;

    padded_width = next_power_of_two(width);
    padded_height = next_power_of_two(height);
    freq = (Complex *)xmalloc(sizeof(Complex) *
                              (size_t)padded_width * (size_t)padded_height);
    centered = float_buffer_create((size_t)width * (size_t)height);
    for (y = 0; y < height; ++y) {
        for (x = 0; x < width; ++x) {
            centered.values[y * width + x] = img->values[y * width + x];
        }
    }
    mean = 0.0;
    for (i = 0; i < (size_t)width * (size_t)height; ++i) {
        mean += centered.values[i];
    }
    mean /= (double)((size_t)width * (size_t)height);
    for (i = 0; i < (size_t)width * (size_t)height; ++i) {
        centered.values[i] -= (float)mean;
    }

    fft2d(&centered, width, height, freq, padded_width, padded_height);
    fft_shift(freq, padded_width, padded_height);

    hist = (double *)xcalloc((size_t)bins, sizeof(double));
    cy = padded_height / 2.0;
    cx = padded_width / 2.0;
    rmin = 0.05 * (double)(width > height ? width : height);

    for (y = 0; y < padded_height; ++y) {
        for (x = 0; x < padded_width; ++x) {
            dy = (double)y - cy;
            dx = (double)x - cx;
            r = sqrt(dy * dy + dx * dx);
            if (r < rmin) {
                continue;
            }
            ang = atan2(dy, dx);
            if (ang < 0.0) {
                ang += M_PI;
            }
            index = (int)(ang / M_PI * bins);
            if (index < 0) {
                index = 0;
            }
            if (index >= bins) {
                index = bins - 1;
            }
            power = freq[y * padded_width + x].re *
                    freq[y * padded_width + x].re +
                    freq[y * padded_width + x].im *
                    freq[y * padded_width + x].im;
            hist[index] += power;
        }
    }

    mean_val = 0.0;
    for (x = 0; x < bins; ++x) {
        mean_val += hist[x];
    }
    mean_val = mean_val / (double)bins + 1e-12;

    max_val = hist[0];
    for (x = 1; x < bins; ++x) {
        if (hist[x] > max_val) {
            max_val = hist[x];
        }
    }

    free(hist);
    free(freq);
    float_buffer_free(&centered);

    return (float)(max_val / mean_val);
}
/*
 * Banding metrics (run-length and gradient-based)
 */
static float
banding_index_runlen(const FloatBuffer *img,
                     int width, int height, int levels)
{
    int y;
    int x;
    double total_runs;
    double total_segments;
    int prev;
    int run_len;
    int segs;
    int runs_sum;
    int value;

    total_runs = 0.0;
    total_segments = 0.0;
    for (y = 0; y < height; ++y) {
        prev = (int)clamp_float(img->values[y * width] * (levels - 1) + 0.5f,
                                0.0f, (float)(levels - 1));
        run_len = 1;
        segs = 0;
        runs_sum = 0;
        for (x = 1; x < width; ++x) {
            value = (int)clamp_float(
                img->values[y * width + x] * (levels - 1) + 0.5f,
                0.0f, (float)(levels - 1));
            if (value == prev) {
                run_len += 1;
            } else {
                runs_sum += run_len;
                segs += 1;
                run_len = 1;
                prev = value;
            }
        }
        runs_sum += run_len;
        segs += 1;
        total_runs += (double)runs_sum / (double)segs;
        total_segments += 1.0;
    }
    if (total_segments <= 0.0) {
        return 0.0f;
    }
    return (float)((total_runs / total_segments) / (double)width);
}

static FloatBuffer
gaussian_blur(const FloatBuffer *img, int width,
              int height, float sigma, int ksize)
{
    FloatBuffer kernel;
    FloatBuffer blurred;

    kernel = gaussian_kernel1d(ksize, sigma);
    blurred = separable_conv2d(img, width, height, &kernel);
    float_buffer_free(&kernel);
    return blurred;
}

static void
finite_diff(const FloatBuffer *img, int width, int height,
            FloatBuffer *dx, FloatBuffer *dy)
{
    int x;
    int y;
    int xm1;
    int xp1;
    int ym1;
    int yp1;
    float vxm1;
    float vxp1;
    float vym1;
    float vyp1;

    *dx = float_buffer_create((size_t)width * (size_t)height);
    *dy = float_buffer_create((size_t)width * (size_t)height);

    for (y = 0; y < height; ++y) {
        for (x = 0; x < width; ++x) {
            xm1 = x - 1;
            xp1 = x + 1;
            ym1 = y - 1;
            yp1 = y + 1;
            if (xm1 < 0) {
                xm1 = 0;
            }
            if (xp1 >= width) {
                xp1 = width - 1;
            }
            if (ym1 < 0) {
                ym1 = 0;
            }
            if (yp1 >= height) {
                yp1 = height - 1;
            }
            vxm1 = img->values[y * width + xm1];
            vxp1 = img->values[y * width + xp1];
            vym1 = img->values[ym1 * width + x];
            vyp1 = img->values[yp1 * width + x];
            dx->values[y * width + x] = (vxp1 - vxm1) * 0.5f;
            dy->values[y * width + x] = (vyp1 - vym1) * 0.5f;
        }
    }
}

static int
compare_floats(const void *a, const void *b)
{
    float fa;
    float fb;
    fa = *(const float *)a;
    fb = *(const float *)b;
    if (fa < fb) {
        return -1;
    }
    if (fa > fb) {
        return 1;
    }
    return 0;
}

static float
banding_index_gradient(const FloatBuffer *img,
                       int width, int height)
{
    FloatBuffer blurred;
    FloatBuffer dx;
    FloatBuffer dy;
    FloatBuffer grad;
    size_t total;
    float *sorted;
    double g99;
    int bins;
    double *hist;
    double *centers;
    int half;
    int i;
    double sum_hist;
    double sum_residual;
    double zero_thresh;
    double zero_mass;
    double sum_x;
    double sum_y;
    double sum_xx;
    double sum_xy;
    int count;
    double slope;
    double intercept;
    double gx;
    double gy;
    size_t idx_pos;
    double value;
    int bin_index;
    double xval;
    double yval;
    double denom;
    double env;
    double resid;

    blurred = gaussian_blur(img, width, height, 1.0f, 7);
    finite_diff(&blurred, width, height, &dx, &dy);
    grad = float_buffer_create((size_t)width * (size_t)height);

    total = (size_t)width * (size_t)height;
    for (i = 0; (size_t)i < total; ++i) {
        gx = dx.values[i];
        gy = dy.values[i];
        grad.values[i] = (float)sqrt(gx * gx + gy * gy);
    }

    sorted = (float *)xmalloc(total * sizeof(float));
    memcpy(sorted, grad.values, total * sizeof(float));
    qsort(sorted, total, sizeof(float), compare_floats);
    if (total == 0) {
        g99 = 0.0;
    } else {
        idx_pos = (size_t)((double)(total - 1) * 0.995);
        g99 = sorted[idx_pos];
    }
    g99 += 1e-9;
    free(sorted);

    bins = 128;
    hist = (double *)xcalloc((size_t)bins, sizeof(double));
    centers = (double *)xmalloc((size_t)bins * sizeof(double));
    for (i = 0; i < bins; ++i) {
        centers[i] = ((double)i + 0.5) * (g99 / (double)bins);
    }

    for (i = 0; (size_t)i < total; ++i) {
        if (grad.values[i] > (float)g99) {
            grad.values[i] = (float)g99;
        }
        value = grad.values[i];
        bin_index = (int)(value / g99 * bins);
        if (bin_index < 0) {
            bin_index = 0;
        }
        if (bin_index >= bins) {
            bin_index = bins - 1;
        }
        hist[bin_index] += 1.0;
    }

    for (i = 0; i < bins; ++i) {
        hist[i] += 1e-12;
    }

    half = bins / 2;
    sum_x = 0.0;
    sum_y = 0.0;
    sum_xx = 0.0;
    sum_xy = 0.0;
    count = bins - half;
    for (i = half; i < bins; ++i) {
        xval = centers[i];
        yval = log(hist[i]);
        sum_x += xval;
        sum_y += yval;
        sum_xx += xval * xval;
        sum_xy += xval * yval;
    }
    slope = 0.0;
    intercept = 0.0;
    if (count > 1) {
        denom = (double)count * sum_xx - sum_x * sum_x;
        if (fabs(denom) > 1e-12) {
            slope = ((double)count * sum_xy - sum_x * sum_y) / denom;
            intercept = (sum_y - slope * sum_x) / (double)count;
        }
    }

    sum_hist = 0.0;
    sum_residual = 0.0;
    for (i = 0; i < bins; ++i) {
        env = exp(intercept + slope * centers[i]);
        resid = hist[i] - env;
        if (resid < 0.0) {
            resid = 0.0;
        }
        sum_hist += hist[i];
        sum_residual += resid;
    }

    zero_thresh = 0.01 * g99;
    zero_mass = 0.0;
    if (total > 0) {
        for (i = 0; (size_t)i < total; ++i) {
            if (grad.values[i] <= (float)zero_thresh) {
                zero_mass += 1.0;
            }
        }
        zero_mass /= (double)total;
    }

    float_buffer_free(&blurred);
    float_buffer_free(&dx);
    float_buffer_free(&dy);
    float_buffer_free(&grad);
    free(hist);
    free(centers);

    if (sum_hist <= 0.0) {
        return 0.0f;
    }
    return (float)(0.6 * (sum_residual / sum_hist) + 0.4 * zero_mass);
}
/*
 * Clipping statistics
 */
static void clipping_rates(const Image *img, float *clip_l, float *clip_r,
                           float *clip_g, float *clip_b)
{
    FloatBuffer luma;
    FloatBuffer rch;
    FloatBuffer gch;
    FloatBuffer bch;
    size_t total;
    size_t i;
    double eps;
    double lo;
    double hi;

    luma = image_to_luma709(img);
    rch = image_channel(img, 0);
    gch = image_channel(img, 1);
    bch = image_channel(img, 2);
    total = (size_t)img->width * (size_t)img->height;
    eps = 1e-6;

    lo = 0.0;
    hi = 0.0;
    for (i = 0; i < total; ++i) {
        if (luma.values[i] <= eps) {
            lo += 1.0;
        }
        if (luma.values[i] >= 1.0 - eps) {
            hi += 1.0;
        }
    }
    *clip_l = (float)((lo + hi) / (double)total);

    lo = hi = 0.0;
    for (i = 0; i < total; ++i) {
        if (rch.values[i] <= eps) {
            lo += 1.0;
        }
        if (rch.values[i] >= 1.0 - eps) {
            hi += 1.0;
        }
    }
    *clip_r = (float)((lo + hi) / (double)total);

    lo = hi = 0.0;
    for (i = 0; i < total; ++i) {
        if (gch.values[i] <= eps) {
            lo += 1.0;
        }
        if (gch.values[i] >= 1.0 - eps) {
            hi += 1.0;
        }
    }
    *clip_g = (float)((lo + hi) / (double)total);

    lo = hi = 0.0;
    for (i = 0; i < total; ++i) {
        if (bch.values[i] <= eps) {
            lo += 1.0;
        }
        if (bch.values[i] >= 1.0 - eps) {
            hi += 1.0;
        }
    }
    *clip_b = (float)((lo + hi) / (double)total);

    float_buffer_free(&luma);
    float_buffer_free(&rch);
    float_buffer_free(&gch);
    float_buffer_free(&bch);
}

/*
 * sRGB <-> CIELAB conversions
 */
static void
srgb_to_linear(const float *src, float *dst, size_t count)
{
    size_t i;
    float c;
    float result;
    for (i = 0; i < count; ++i) {
        c = src[i];
        if (c <= 0.04045f) {
            result = c / 12.92f;
        } else {
            result = powf((c + 0.055f) / 1.055f, 2.4f);
        }
        dst[i] = result;
    }
}

static void
linear_to_xyz(const float *rgb, float *xyz, size_t pixels)
{
    size_t i;
    float r;
    float g;
    float b;
    float X;
    float Y;
    float Z;
    for (i = 0; i < pixels; ++i) {
        r = rgb[i * 3 + 0];
        g = rgb[i * 3 + 1];
        b = rgb[i * 3 + 2];
        X = 0.4124564f * r + 0.3575761f * g + 0.1804375f * b;
        Y = 0.2126729f * r + 0.7151522f * g + 0.0721750f * b;
        Z = 0.0193339f * r + 0.1191920f * g + 0.9503041f * b;
        xyz[i * 3 + 0] = X;
        xyz[i * 3 + 1] = Y;
        xyz[i * 3 + 2] = Z;
    }
}

static float
f_lab(float t)
{
    float delta;
    delta = 6.0f / 29.0f;
    if (t > delta * delta * delta) {
        return cbrtf(t);
    }
    return t / (3.0f * delta * delta) + 4.0f / 29.0f;
}

static void
xyz_to_lab(const float *xyz, float *lab, size_t pixels)
{
    const float Xn = 0.95047f;
    const float Yn = 1.00000f;
    const float Zn = 1.08883f;
    size_t i;
    float X;
    float Y;
    float Z;
    float fx;
    float fy;
    float fz;
    float L;
    float a;
    float b;
    for (i = 0; i < pixels; ++i) {
        X = xyz[i * 3 + 0] / Xn;
        Y = xyz[i * 3 + 1] / Yn;
        Z = xyz[i * 3 + 2] / Zn;
        fx = f_lab(X);
        fy = f_lab(Y);
        fz = f_lab(Z);
        L = 116.0f * fy - 16.0f;
        a = 500.0f * (fx - fy);
        b = 200.0f * (fy - fz);
        lab[i * 3 + 0] = L;
        lab[i * 3 + 1] = a;
        lab[i * 3 + 2] = b;
    }
}

static FloatBuffer
rgb_to_lab(const Image *img)
{
    FloatBuffer lab;
    float *linear;
    float *xyz;
    size_t pixels;

    pixels = (size_t)img->width * (size_t)img->height;
    lab = float_buffer_create(pixels * 3);
    linear = (float *)xmalloc(pixels * 3 * sizeof(float));
    xyz = (float *)xmalloc(pixels * 3 * sizeof(float));
    srgb_to_linear(img->pixels, linear, pixels * 3);
    linear_to_xyz(linear, xyz, pixels);
    xyz_to_lab(xyz, lab.values, pixels);
    free(linear);
    free(xyz);
    return lab;
}

static FloatBuffer
chroma_ab(const FloatBuffer *lab, size_t pixels)
{
    FloatBuffer chroma;
    size_t i;
    float a;
    float b;
    chroma = float_buffer_create(pixels);
    for (i = 0; i < pixels; ++i) {
        a = lab->values[i * 3 + 1];
        b = lab->values[i * 3 + 2];
        chroma.values[i] = sqrtf(a * a + b * b);
    }
    return chroma;
}

static FloatBuffer
deltaE00(const FloatBuffer *lab1,
         const FloatBuffer *lab2, size_t pixels)
{
    FloatBuffer out;
    size_t i;
    double L1;
    double a1;
    double b1;
    double L2;
    double a2;
    double b2;
    double Lbar;
    double C1;
    double C2;
    double Cbar;
    double G;
    double a1p;
    double a2p;
    double C1p;
    double C2p;
    double h1p;
    double h2p;
    double dLp;
    double dCp;
    double dhp;
    double dHp;
    double Lpm;
    double Cpm;
    double hp_sum;
    double hp_diff;
    double Hpm;
    double T;
    double Sl;
    double Sc;
    double Sh;
    double dTheta;
    double Rc;
    double Rt;
    double de;
    out = float_buffer_create(pixels);
    for (i = 0; i < pixels; ++i) {
        L1 = lab1->values[i * 3 + 0];
        a1 = lab1->values[i * 3 + 1];
        b1 = lab1->values[i * 3 + 2];
        L2 = lab2->values[i * 3 + 0];
        a2 = lab2->values[i * 3 + 1];
        b2 = lab2->values[i * 3 + 2];
        Lbar = 0.5 * (L1 + L2);
        C1 = sqrt(a1 * a1 + b1 * b1);
        C2 = sqrt(a2 * a2 + b2 * b2);
        Cbar = 0.5 * (C1 + C2);
        G = 0.5 * (1.0 - sqrt(pow(Cbar, 7.0) /
                               (pow(Cbar, 7.0) + pow(25.0, 7.0) + 1e-12)));
        a1p = (1.0 + G) * a1;
        a2p = (1.0 + G) * a2;
        C1p = sqrt(a1p * a1p + b1 * b1);
        C2p = sqrt(a2p * a2p + b2 * b2);
        h1p = atan2(b1, a1p);
        if (h1p < 0.0) {
            h1p += 2.0 * M_PI;
        }
        h2p = atan2(b2, a2p);
        if (h2p < 0.0) {
            h2p += 2.0 * M_PI;
        }
        dLp = L2 - L1;
        dCp = C2p - C1p;
        dhp = h2p - h1p;
        if (dhp > M_PI) {
            dhp -= 2.0 * M_PI;
        }
        if (dhp < -M_PI) {
            dhp += 2.0 * M_PI;
        }
        dHp = 2.0 * sqrt(C1p * C2p + 1e-12) * sin(dhp / 2.0);
        Lpm = Lbar;
        Cpm = 0.5 * (C1p + C2p);
        hp_sum = h1p + h2p;
        hp_diff = fabs(h1p - h2p);
        if (C1p * C2p == 0.0) {
            Hpm = hp_sum;
        } else {
            if (hp_diff <= M_PI) {
                Hpm = 0.5 * hp_sum;
            } else {
                if (hp_sum < 2.0 * M_PI) {
                    Hpm = 0.5 * (hp_sum + 2.0 * M_PI);
                } else {
                    Hpm = 0.5 * (hp_sum - 2.0 * M_PI);
                }
            }
        }
        T = 1.0 - 0.17 * cos(Hpm - M_PI / 6.0) +
            0.24 * cos(2.0 * Hpm) +
            0.32 * cos(3.0 * Hpm + M_PI / 30.0) -
            0.20 * cos(4.0 * Hpm - 7.0 * M_PI / 20.0);
        Sl = 1.0 + ((0.015 * pow(Lpm - 50.0, 2.0)) /
                    sqrt(20.0 + pow(Lpm - 50.0, 2.0)));
        Sc = 1.0 + 0.045 * Cpm;
        Sh = 1.0 + 0.015 * Cpm * T;
        dTheta = 30.0 * exp(-pow((Hpm - 275.0 * M_PI / 180.0) /
                                 (25.0 * M_PI / 180.0), 2.0));
        Rc = 2.0 * sqrt(pow(Cpm, 7.0) /
                        (pow(Cpm, 7.0) + pow(25.0, 7.0) + 1e-12));
        Rt = -Rc * sin(2.0 * dTheta * M_PI / 180.0);
        de = sqrt(pow(dLp / (Sl), 2.0) + pow(dCp / (Sc), 2.0) +
                  pow(dHp / (Sh), 2.0) +
                  Rt * (dCp / Sc) * (dHp / Sh));
        out.values[i] = (float)de;
    }
    return out;
}
/*
 * GMSD and PSNR
 */
static float
gmsd_metric(const FloatBuffer *ref, const FloatBuffer *out,
            int width, int height)
{
    static const float kx[9] = {0.25f, 0.0f, -0.25f,
                                0.5f, 0.0f, -0.5f,
                                0.25f, 0.0f, -0.25f};
    static const float ky[9] = {0.25f, 0.5f, 0.25f,
                                0.0f, 0.0f, 0.0f,
                                -0.25f, -0.5f, -0.25f};
    FloatBuffer gx1;
    FloatBuffer gy1;
    FloatBuffer gx2;
    FloatBuffer gy2;
    FloatBuffer gm1;
    FloatBuffer gm2;
    double c;
    double mean;
    double sq_sum;
    size_t total;
    int y;
    int x;
    float accx1;
    float accy1;
    float accx2;
    float accy2;
    int dy;
    int dx;
    int yy;
    int xx;
    int kidx;
    size_t idx;
    float mag1;
    float mag2;
    double gms;

    gx1 = float_buffer_create((size_t)width * (size_t)height);
    gy1 = float_buffer_create((size_t)width * (size_t)height);
    gx2 = float_buffer_create((size_t)width * (size_t)height);
    gy2 = float_buffer_create((size_t)width * (size_t)height);

    for (y = 0; y < height; ++y) {
        for (x = 0; x < width; ++x) {
            accx1 = 0.0f;
            accy1 = 0.0f;
            accx2 = 0.0f;
            accy2 = 0.0f;
            for (dy = -1; dy <= 1; ++dy) {
                yy = y + dy;
                if (yy < 0) {
                    yy = -yy;
                }
                if (yy >= height) {
                    yy = height - (yy - height) - 1;
                    if (yy < 0) {
                        yy = 0;
                    }
                }
                for (dx = -1; dx <= 1; ++dx) {
                    xx = x + dx;
                    if (xx < 0) {
                        xx = -xx;
                    }
                    if (xx >= width) {
                        xx = width - (xx - width) - 1;
                        if (xx < 0) {
                            xx = 0;
                        }
                    }
                    kidx = (dy + 1) * 3 + (dx + 1);
                    accx1 += ref->values[yy * width + xx] * kx[kidx];
                    accy1 += ref->values[yy * width + xx] * ky[kidx];
                    accx2 += out->values[yy * width + xx] * kx[kidx];
                    accy2 += out->values[yy * width + xx] * ky[kidx];
                }
            }
            gx1.values[y * width + x] = accx1;
            gy1.values[y * width + x] = accy1;
            gx2.values[y * width + x] = accx2;
            gy2.values[y * width + x] = accy2;
        }
    }

    gm1 = float_buffer_create((size_t)width * (size_t)height);
    gm2 = float_buffer_create((size_t)width * (size_t)height);
    total = (size_t)width * (size_t)height;
    for (y = 0; y < height; ++y) {
        for (x = 0; x < width; ++x) {
            idx = (size_t)y * (size_t)width + (size_t)x;
            mag1 = sqrtf(gx1.values[idx] * gx1.values[idx] +
                         gy1.values[idx] * gy1.values[idx]) + 1e-12f;
            mag2 = sqrtf(gx2.values[idx] * gx2.values[idx] +
                         gy2.values[idx] * gy2.values[idx]) + 1e-12f;
            gm1.values[idx] = mag1;
            gm2.values[idx] = mag2;
        }
    }

    c = 0.0026;
    mean = 0.0;
    for (y = 0; y < height; ++y) {
        for (x = 0; x < width; ++x) {
            idx = (size_t)y * (size_t)width + (size_t)x;
            gms = (2.0 * gm1.values[idx] * gm2.values[idx] + c) /
                  (gm1.values[idx] * gm1.values[idx] +
                   gm2.values[idx] * gm2.values[idx] + c);
            mean += gms;
        }
    }
    mean /= (double)total;

    sq_sum = 0.0;
    for (y = 0; y < height; ++y) {
        for (x = 0; x < width; ++x) {
            idx = (size_t)y * (size_t)width + (size_t)x;
            gms = (2.0 * gm1.values[idx] * gm2.values[idx] + c) /
                  (gm1.values[idx] * gm1.values[idx] +
                   gm2.values[idx] * gm2.values[idx] + c);
            sq_sum += (gms - mean) * (gms - mean);
        }
    }
    sq_sum /= (double)total;

    float_buffer_free(&gx1);
    float_buffer_free(&gy1);
    float_buffer_free(&gx2);
    float_buffer_free(&gy2);
    float_buffer_free(&gm1);
    float_buffer_free(&gm2);

    return (float)sqrt(sq_sum);
}

static float
psnr_metric(const FloatBuffer *ref, const FloatBuffer *out,
            int width, int height)
{
    double mse;
    size_t total;
    size_t i;
    double diff;

    mse = 0.0;
    total = (size_t)width * (size_t)height;
    for (i = 0; i < total; ++i) {
        diff = ref->values[i] - out->values[i];
        mse += diff * diff;
    }
    mse /= (double)total;
    if (mse <= 1e-12) {
        return 99.0f;
    }
    return (float)(10.0 * log10(1.0 / mse));
}

/*
 * Metrics aggregation
 */
static Metrics evaluate_metrics(const Image *ref_img, const Image *out_img)
{
    Metrics metrics;
    FloatBuffer ref_luma;
    FloatBuffer out_luma;
    FloatBuffer ref_lab;
    FloatBuffer out_lab;
    FloatBuffer ref_chroma;
    FloatBuffer out_chroma;
    FloatBuffer de00;
    size_t pixels;
    size_t iter;
    double sum_value;

    memset(&metrics, 0, sizeof(metrics));
    metrics.lpips_vgg = NAN;

    ref_luma = image_to_luma709(ref_img);
    out_luma = image_to_luma709(out_img);

    metrics.ms_ssim = ms_ssim_luma(&ref_luma, &out_luma,
                                   ref_img->width, ref_img->height);

    metrics.high_freq_out = high_frequency_ratio(&out_luma,
                                                 out_img->width,
                                                 out_img->height, 0.25f);
    metrics.high_freq_ref = high_frequency_ratio(&ref_luma,
                                                 ref_img->width,
                                                 ref_img->height, 0.25f);
    metrics.high_freq_delta = metrics.high_freq_out - metrics.high_freq_ref;

    metrics.stripe_ref = stripe_score(&ref_luma, ref_img->width,
                                      ref_img->height, 180);
    metrics.stripe_out = stripe_score(&out_luma, out_img->width,
                                      out_img->height, 180);
    metrics.stripe_rel = metrics.stripe_out - metrics.stripe_ref;

    metrics.band_run_rel = banding_index_runlen(&out_luma, out_img->width,
                                                out_img->height, 32) -
                           banding_index_runlen(&ref_luma, ref_img->width,
                                                ref_img->height, 32);

    metrics.band_grad_rel = banding_index_gradient(&out_luma,
                                                   out_img->width,
                                                   out_img->height) -
                            banding_index_gradient(&ref_luma,
                                                   ref_img->width,
                                                   ref_img->height);

    clipping_rates(ref_img, &metrics.clip_l_ref, &metrics.clip_r_ref,
                   &metrics.clip_g_ref, &metrics.clip_b_ref);
    clipping_rates(out_img, &metrics.clip_l_out, &metrics.clip_r_out,
                   &metrics.clip_g_out, &metrics.clip_b_out);

    metrics.clip_l_rel = metrics.clip_l_out - metrics.clip_l_ref;
    metrics.clip_r_rel = metrics.clip_r_out - metrics.clip_r_ref;
    metrics.clip_g_rel = metrics.clip_g_out - metrics.clip_g_ref;
    metrics.clip_b_rel = metrics.clip_b_out - metrics.clip_b_ref;

    ref_lab = rgb_to_lab(ref_img);
    out_lab = rgb_to_lab(out_img);
    pixels = (size_t)ref_img->width * (size_t)ref_img->height;
    ref_chroma = chroma_ab(&ref_lab, pixels);
    out_chroma = chroma_ab(&out_lab, pixels);

    sum_value = 0.0;
    for (iter = 0; iter < pixels; ++iter) {
        sum_value += fabs(out_chroma.values[iter] -
                          ref_chroma.values[iter]);
    }
    metrics.delta_chroma_mean = (float)(sum_value / (double)pixels);

    de00 = deltaE00(&ref_lab, &out_lab, pixels);
    sum_value = 0.0;
    for (iter = 0; iter < pixels; ++iter) {
        sum_value += de00.values[iter];
    }
    metrics.delta_e00_mean = (float)(sum_value / (double)pixels);

    metrics.gmsd_value = gmsd_metric(&ref_luma, &out_luma,
                                     ref_img->width, ref_img->height);
    metrics.psnr_y = psnr_metric(&ref_luma, &out_luma,
                                 ref_img->width, ref_img->height);

    float_buffer_free(&ref_luma);
    float_buffer_free(&out_luma);
    float_buffer_free(&ref_lab);
    float_buffer_free(&out_lab);
    float_buffer_free(&ref_chroma);
    float_buffer_free(&out_chroma);
    float_buffer_free(&de00);

    return metrics;
}

/*
 * LPIPS metric integration (ONNX Runtime)
 */
static float
compute_lpips_vgg(sixel_assessment_t *assessment,
                  const Image *ref_img,
                  const Image *out_img)
{
    float value;

    value = NAN;
#if defined(HAVE_ONNXRUNTIME)
    image_f32_t ref_tensor;
    image_f32_t out_tensor;
    float distance;

    ref_tensor.width = 0;
    ref_tensor.height = 0;
    ref_tensor.nchw = NULL;
    out_tensor = ref_tensor;
    distance = NAN;

    if (assessment->enable_lpips == 0) {
        goto done;
    }
    if (convert_image_to_nchw(ref_img, &ref_tensor) != 0) {
        fprintf(stderr,
                "Warning: unable to convert reference image for LPIPS.\n");
        goto done;
    }
    if (convert_image_to_nchw(out_img, &out_tensor) != 0) {
        fprintf(stderr,
                "Warning: unable to convert output image for LPIPS.\n");
        goto done;
    }
    if (ensure_lpips_models(assessment) != 0) {
        goto done;
    }
    if (run_lpips(assessment->diff_model_path,
                  assessment->feat_model_path,
                  &ref_tensor,
                  &out_tensor,
                  &distance) != 0) {
        goto done;
    }
    value = distance;

done:
    free_image_f32(&ref_tensor);
    free_image_f32(&out_tensor);
#else
    (void)assessment;
    (void)ref_img;
    (void)out_img;
#endif
    return value;
}

static void
align_images(Image *ref, Image *out)
{
    int width;
    int height;
    int channels;
    float *ref_new;
    float *out_new;
    int y;
    size_t row_bytes;

    width = ref->width < out->width ? ref->width : out->width;
    height = ref->height < out->height ? ref->height : out->height;
    channels = ref->channels;
    if (channels != out->channels) {
        assessment_fail(SIXEL_BAD_ARGUMENT,
                       "Channel mismatch between input frames");
    }
    ref_new = (float *)xmalloc((size_t)width * (size_t)height *
                               (size_t)channels * sizeof(float));
    out_new = (float *)xmalloc((size_t)width * (size_t)height *
                               (size_t)channels * sizeof(float));
    row_bytes = (size_t)width * (size_t)channels * sizeof(float);
    for (y = 0; y < height; ++y) {
        memcpy(ref_new + (size_t)y * (size_t)width * (size_t)channels,
               ref->pixels + (size_t)y * (size_t)ref->width *
               (size_t)channels, row_bytes);
        memcpy(out_new + (size_t)y * (size_t)width * (size_t)channels,
               out->pixels + (size_t)y * (size_t)out->width *
               (size_t)channels, row_bytes);
    }
    free(ref->pixels);
    free(out->pixels);
    ref->pixels = ref_new;
    out->pixels = out_new;
    ref->width = width;
    ref->height = height;
    out->width = width;
    out->height = height;
}

/*
 * Assessment API bridge
 */
typedef struct MetricDescriptor {
    int id;
    const char *json_key;
} MetricDescriptor;

static const MetricDescriptor g_metric_table[] = {
    {SIXEL_ASSESSMENT_METRIC_MS_SSIM, "MS-SSIM"},
    {SIXEL_ASSESSMENT_METRIC_HIGH_FREQ_OUT, "HighFreqRatio_out"},
    {SIXEL_ASSESSMENT_METRIC_HIGH_FREQ_REF, "HighFreqRatio_ref"},
    {SIXEL_ASSESSMENT_METRIC_HIGH_FREQ_DELTA, "HighFreqRatio_delta"},
    {SIXEL_ASSESSMENT_METRIC_STRIPE_REF, "StripeScore_ref"},
    {SIXEL_ASSESSMENT_METRIC_STRIPE_OUT, "StripeScore_out"},
    {SIXEL_ASSESSMENT_METRIC_STRIPE_REL, "StripeScore_rel"},
    {SIXEL_ASSESSMENT_METRIC_BAND_RUN_REL, "BandingIndex_rel"},
    {SIXEL_ASSESSMENT_METRIC_BAND_GRAD_REL, "BandingIndex_grad_rel"},
    {SIXEL_ASSESSMENT_METRIC_CLIP_L_REF, "ClipRate_L_ref"},
    {SIXEL_ASSESSMENT_METRIC_CLIP_R_REF, "ClipRate_R_ref"},
    {SIXEL_ASSESSMENT_METRIC_CLIP_G_REF, "ClipRate_G_ref"},
    {SIXEL_ASSESSMENT_METRIC_CLIP_B_REF, "ClipRate_B_ref"},
    {SIXEL_ASSESSMENT_METRIC_CLIP_L_OUT, "ClipRate_L_out"},
    {SIXEL_ASSESSMENT_METRIC_CLIP_R_OUT, "ClipRate_R_out"},
    {SIXEL_ASSESSMENT_METRIC_CLIP_G_OUT, "ClipRate_G_out"},
    {SIXEL_ASSESSMENT_METRIC_CLIP_B_OUT, "ClipRate_B_out"},
    {SIXEL_ASSESSMENT_METRIC_CLIP_L_REL, "ClipRate_L_rel"},
    {SIXEL_ASSESSMENT_METRIC_CLIP_R_REL, "ClipRate_R_rel"},
    {SIXEL_ASSESSMENT_METRIC_CLIP_G_REL, "ClipRate_G_rel"},
    {SIXEL_ASSESSMENT_METRIC_CLIP_B_REL, "ClipRate_B_rel"},
    {SIXEL_ASSESSMENT_METRIC_DELTA_CHROMA, "Δ Chroma_mean"},
    {SIXEL_ASSESSMENT_METRIC_DELTA_E00, "Δ E00_mean"},
    {SIXEL_ASSESSMENT_METRIC_GMSD, "GMSD"},
    {SIXEL_ASSESSMENT_METRIC_PSNR_Y, "PSNR_Y"},
    {SIXEL_ASSESSMENT_METRIC_LPIPS_VGG, "LPIPS(vgg)"},
};

static void
store_metrics(sixel_assessment_t *assessment, const Metrics *metrics)
{
    double *results;

    results = assessment->results;
    results[SIXEL_ASSESSMENT_INDEX(SIXEL_ASSESSMENT_METRIC_MS_SSIM)]
        = metrics->ms_ssim;
    results[SIXEL_ASSESSMENT_INDEX(SIXEL_ASSESSMENT_METRIC_HIGH_FREQ_OUT)]
        = metrics->high_freq_out;
    results[SIXEL_ASSESSMENT_INDEX(SIXEL_ASSESSMENT_METRIC_HIGH_FREQ_REF)]
        = metrics->high_freq_ref;
    results[SIXEL_ASSESSMENT_INDEX(SIXEL_ASSESSMENT_METRIC_HIGH_FREQ_DELTA)]
        = metrics->high_freq_delta;
    results[SIXEL_ASSESSMENT_INDEX(SIXEL_ASSESSMENT_METRIC_STRIPE_REF)]
        = metrics->stripe_ref;
    results[SIXEL_ASSESSMENT_INDEX(SIXEL_ASSESSMENT_METRIC_STRIPE_OUT)]
        = metrics->stripe_out;
    results[SIXEL_ASSESSMENT_INDEX(SIXEL_ASSESSMENT_METRIC_STRIPE_REL)]
        = metrics->stripe_rel;
    results[SIXEL_ASSESSMENT_INDEX(SIXEL_ASSESSMENT_METRIC_BAND_RUN_REL)]
        = metrics->band_run_rel;
    results[SIXEL_ASSESSMENT_INDEX(SIXEL_ASSESSMENT_METRIC_BAND_GRAD_REL)]
        = metrics->band_grad_rel;
    results[SIXEL_ASSESSMENT_INDEX(SIXEL_ASSESSMENT_METRIC_CLIP_L_REF)]
        = metrics->clip_l_ref;
    results[SIXEL_ASSESSMENT_INDEX(SIXEL_ASSESSMENT_METRIC_CLIP_R_REF)]
        = metrics->clip_r_ref;
    results[SIXEL_ASSESSMENT_INDEX(SIXEL_ASSESSMENT_METRIC_CLIP_G_REF)]
        = metrics->clip_g_ref;
    results[SIXEL_ASSESSMENT_INDEX(SIXEL_ASSESSMENT_METRIC_CLIP_B_REF)]
        = metrics->clip_b_ref;
    results[SIXEL_ASSESSMENT_INDEX(SIXEL_ASSESSMENT_METRIC_CLIP_L_OUT)]
        = metrics->clip_l_out;
    results[SIXEL_ASSESSMENT_INDEX(SIXEL_ASSESSMENT_METRIC_CLIP_R_OUT)]
        = metrics->clip_r_out;
    results[SIXEL_ASSESSMENT_INDEX(SIXEL_ASSESSMENT_METRIC_CLIP_G_OUT)]
        = metrics->clip_g_out;
    results[SIXEL_ASSESSMENT_INDEX(SIXEL_ASSESSMENT_METRIC_CLIP_B_OUT)]
        = metrics->clip_b_out;
    results[SIXEL_ASSESSMENT_INDEX(SIXEL_ASSESSMENT_METRIC_CLIP_L_REL)]
        = metrics->clip_l_rel;
    results[SIXEL_ASSESSMENT_INDEX(SIXEL_ASSESSMENT_METRIC_CLIP_R_REL)]
        = metrics->clip_r_rel;
    results[SIXEL_ASSESSMENT_INDEX(SIXEL_ASSESSMENT_METRIC_CLIP_G_REL)]
        = metrics->clip_g_rel;
    results[SIXEL_ASSESSMENT_INDEX(SIXEL_ASSESSMENT_METRIC_CLIP_B_REL)]
        = metrics->clip_b_rel;
    results[SIXEL_ASSESSMENT_INDEX(SIXEL_ASSESSMENT_METRIC_DELTA_CHROMA)]
        = metrics->delta_chroma_mean;
    results[SIXEL_ASSESSMENT_INDEX(SIXEL_ASSESSMENT_METRIC_DELTA_E00)]
        = metrics->delta_e00_mean;
    results[SIXEL_ASSESSMENT_INDEX(SIXEL_ASSESSMENT_METRIC_GMSD)]
        = metrics->gmsd_value;
    results[SIXEL_ASSESSMENT_INDEX(SIXEL_ASSESSMENT_METRIC_PSNR_Y)]
        = metrics->psnr_y;
    results[SIXEL_ASSESSMENT_INDEX(SIXEL_ASSESSMENT_METRIC_LPIPS_VGG)]
        = metrics->lpips_vgg;
}

static SIXELSTATUS
sixel_assessment_capture_first_frame(sixel_frame_t *frame,
                                     void *user_data)
{
    sixel_assessment_capture_t *capture;

    /*
     * Loader pipeline sketch for encoded round trips:
     *
     *     +--------------+     +-----------------+
     *     | decoder loop | --> | capture.frame   |
     *     +--------------+     +-----------------+
     */

    capture = (sixel_assessment_capture_t *)user_data;
    if (capture == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    if (capture->frame == NULL) {
        sixel_frame_ref(frame);
        capture->frame = frame;
    }
    return SIXEL_OK;
}

SIXELSTATUS
sixel_assessment_expand_quantized_frame(sixel_frame_t *source,
                                        sixel_allocator_t *allocator,
                                        sixel_frame_t **ppframe)
{
    SIXELSTATUS status;
    sixel_frame_t *frame;
    unsigned char *indices;
    unsigned char const *palette;
    unsigned char *rgb_pixels;
    size_t pixel_count;
    size_t rgb_bytes;
    size_t palette_index;
    size_t palette_offset;
    size_t i;
    int width;
    int height;
    int ncolors;
    int colorspace;
    int pixelformat;
    unsigned char *dst;

    /*
     * Convert the paletted capture into RGB triplets so that the
     * assessment pipeline can compare against the original input in
     * a like-for-like space.
     *
     *     +-----------+     +----------------+
     *     | indices   | --> | RGB triplets   |
     *     +-----------+     +----------------+
     */

    status = SIXEL_FALSE;
    frame = NULL;
    indices = NULL;
    palette = NULL;
    rgb_pixels = NULL;
    pixel_count = 0;
    rgb_bytes = 0;
    palette_index = 0;
    palette_offset = 0;
    i = 0;
    width = 0;
    height = 0;
    ncolors = 0;
    colorspace = SIXEL_COLORSPACE_GAMMA;
    pixelformat = SIXEL_PIXELFORMAT_RGB888;
    dst = NULL;

    if (source == NULL || allocator == NULL || ppframe == NULL) {
        sixel_helper_set_additional_message(
            "sixel_assessment_expand_quantized_frame: invalid argument.");
        return SIXEL_BAD_ARGUMENT;
    }

    *ppframe = NULL;
    indices = (unsigned char *)sixel_frame_get_pixels(source);
    palette = (unsigned char const *)sixel_frame_get_palette(source);
    width = sixel_frame_get_width(source);
    height = sixel_frame_get_height(source);
    ncolors = sixel_frame_get_ncolors(source);
    pixelformat = sixel_frame_get_pixelformat(source);
    colorspace = sixel_frame_get_colorspace(source);

    if (pixelformat != SIXEL_PIXELFORMAT_PAL8) {
        sixel_helper_set_additional_message(
            "sixel_assessment_expand_quantized_frame: not paletted data.");
        return SIXEL_RUNTIME_ERROR;
    }
    if (indices == NULL || palette == NULL || width <= 0 || height <= 0) {
        sixel_helper_set_additional_message(
            "sixel_assessment_expand_quantized_frame: capture incomplete.");
        return SIXEL_RUNTIME_ERROR;
    }
    pixel_count = (size_t)width * (size_t)height;
    if (height != 0 && pixel_count / (size_t)height != (size_t)width) {
        sixel_helper_set_additional_message(
            "sixel_assessment_expand_quantized_frame: size overflow.");
        return SIXEL_RUNTIME_ERROR;
    }
    if (ncolors <= 0 || ncolors > 256) {
        sixel_helper_set_additional_message(
            "sixel_assessment_expand_quantized_frame: palette size invalid.");
        return SIXEL_RUNTIME_ERROR;
    }

    rgb_bytes = pixel_count * 3u;
    if (pixel_count != 0 && rgb_bytes / 3u != pixel_count) {
        sixel_helper_set_additional_message(
            "sixel_assessment_expand_quantized_frame: RGB overflow.");
        return SIXEL_RUNTIME_ERROR;
    }

    status = sixel_frame_new(&frame, allocator);
    if (SIXEL_FAILED(status)) {
        return status;
    }

    rgb_pixels = (unsigned char *)sixel_allocator_malloc(allocator,
                                                         rgb_bytes);
    if (rgb_pixels == NULL) {
        sixel_helper_set_additional_message(
            "sixel_assessment_expand_quantized_frame: malloc failed.");
        status = SIXEL_BAD_ALLOCATION;
        goto cleanup;
    }

    dst = rgb_pixels;
    for (i = 0; i < pixel_count; ++i) {
        palette_index = (size_t)indices[i];
        if (palette_index >= (size_t)ncolors) {
            sixel_helper_set_additional_message(
                "sixel_assessment_expand_quantized_frame: index overflow.");
            status = SIXEL_RUNTIME_ERROR;
            goto cleanup;
        }
        palette_offset = palette_index * 3u;
        dst[0] = palette[palette_offset + 0];
        dst[1] = palette[palette_offset + 1];
        dst[2] = palette[palette_offset + 2];
        dst += 3;
    }

    status = sixel_frame_init(frame,
                              rgb_pixels,
                              width,
                              height,
                              SIXEL_PIXELFORMAT_RGB888,
                              NULL,
                              0);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }

    rgb_pixels = NULL;
    status = sixel_frame_ensure_colorspace(frame, colorspace);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }

    *ppframe = frame;
    return SIXEL_OK;

cleanup:
    if (rgb_pixels != NULL) {
        sixel_allocator_free(allocator, rgb_pixels);
    }
    if (frame != NULL) {
        sixel_frame_unref(frame);
    }
    return status;
}

SIXELSTATUS
sixel_assessment_load_single_frame(char const *path,
                                   sixel_allocator_t *allocator,
                                   sixel_frame_t **ppframe)
{
    SIXELSTATUS status;
    sixel_loader_t *loader;
    sixel_assessment_capture_t capture;
    int fstatic;
    int fuse_palette;
    int reqcolors;
    int loop_override;
    int finsecure;

    /*
     * Reload a single-frame image with the regular loader stack.  This helper
     * is used when the encoded SIXEL output needs to be analyzed by the
     * assessment pipeline.
     */

    status = SIXEL_FALSE;
    loader = NULL;
    capture.frame = NULL;
    fstatic = 1;
    fuse_palette = 0;
    reqcolors = SIXEL_PALETTE_MAX;
    loop_override = SIXEL_LOOP_DISABLE;
    finsecure = 0;

    if (path == NULL || allocator == NULL || ppframe == NULL) {
        sixel_helper_set_additional_message(
            "sixel_assessment_load_single_frame: invalid argument.");
        return SIXEL_BAD_ARGUMENT;
    }

    status = sixel_loader_new(&loader, allocator);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }

    status = sixel_loader_setopt(loader,
                                 SIXEL_LOADER_OPTION_REQUIRE_STATIC,
                                 &fstatic);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }

    status = sixel_loader_setopt(loader,
                                 SIXEL_LOADER_OPTION_USE_PALETTE,
                                 &fuse_palette);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }

    status = sixel_loader_setopt(loader,
                                 SIXEL_LOADER_OPTION_REQCOLORS,
                                 &reqcolors);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }

    status = sixel_loader_setopt(loader,
                                 SIXEL_LOADER_OPTION_LOOP_CONTROL,
                                 &loop_override);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }

    status = sixel_loader_setopt(loader,
                                 SIXEL_LOADER_OPTION_INSECURE,
                                 &finsecure);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }

    status = sixel_loader_setopt(loader,
                                 SIXEL_LOADER_OPTION_CONTEXT,
                                 &capture);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }

    status = sixel_loader_load_file(loader,
                                    path,
                                    sixel_assessment_capture_first_frame);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }
    if (capture.frame == NULL) {
        sixel_helper_set_additional_message(
            "sixel_assessment_load_single_frame: no frames captured.");
        status = SIXEL_RUNTIME_ERROR;
        goto cleanup;
    }

    sixel_frame_ref(capture.frame);
    *ppframe = capture.frame;
    capture.frame = NULL;
    status = SIXEL_OK;

cleanup:
    if (capture.frame != NULL) {
        sixel_frame_unref(capture.frame);
    }
    if (loader != NULL) {
        sixel_loader_unref(loader);
    }
    return status;
}

SIXELSTATUS
sixel_assessment_new(sixel_assessment_t **ppassessment,
                    sixel_allocator_t *allocator)
{
    SIXELSTATUS status;
    sixel_assessment_t *assessment;

    if (ppassessment == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    if (allocator == NULL) {
        status = sixel_allocator_new(&allocator,
                                     NULL,
                                     NULL,
                                     NULL,
                                     NULL);
        if (SIXEL_FAILED(status)) {
            return status;
        }
    } else {
        sixel_allocator_ref(allocator);
    }

    assessment = (sixel_assessment_t *)sixel_allocator_malloc(
        allocator,
        sizeof(sixel_assessment_t));
    if (assessment == NULL) {
        sixel_allocator_unref(allocator);
        return SIXEL_BAD_ALLOCATION;
    }

    assessment->refcount = 1;
    assessment->allocator = allocator;
    assessment->enable_lpips = 1;
    assessment->results_ready = 0;
    assessment->last_error = SIXEL_OK;
    assessment->error_message[0] = '\0';
    assessment->binary_dir[0] = '\0';
    assessment->binary_dir_state = 0;
    assessment->model_dir[0] = '\0';
    assessment->model_dir_state = 0;
    assessment->lpips_models_ready = 0;
    assessment->diff_model_path[0] = '\0';
    assessment->feat_model_path[0] = '\0';
    memset(assessment->results, 0,
           sizeof(assessment->results));

    *ppassessment = assessment;
    return SIXEL_OK;
}

void
sixel_assessment_ref(sixel_assessment_t *assessment)
{
    if (assessment == NULL) {
        return;
    }
    assessment->refcount += 1;
}

void
sixel_assessment_unref(sixel_assessment_t *assessment)
{
    if (assessment == NULL) {
        return;
    }
    assessment->refcount -= 1;
    if (assessment->refcount == 0) {
        sixel_allocator_t *allocator;

        allocator = assessment->allocator;
        sixel_allocator_free(allocator, assessment);
        sixel_allocator_unref(allocator);
    }
}

static int
parse_bool_option(char const *value, int *out)
{
    char lowered[8];
    size_t len;
    size_t i;

    if (value == NULL || out == NULL) {
        return -1;
    }
    len = strlen(value);
    if (len >= sizeof(lowered)) {
        return -1;
    }
    for (i = 0; i < len; ++i) {
        lowered[i] = (char)tolower((unsigned char)value[i]);
    }
    lowered[len] = '\0';
    if (strcmp(lowered, "1") == 0 || strcmp(lowered, "true") == 0 ||
        strcmp(lowered, "yes") == 0) {
        *out = 1;
        return 0;
    }
    if (strcmp(lowered, "0") == 0 || strcmp(lowered, "false") == 0 ||
        strcmp(lowered, "no") == 0) {
        *out = 0;
        return 0;
    }
    return -1;
}

SIXELSTATUS
sixel_assessment_setopt(sixel_assessment_t *assessment,
                       int option,
                       char const *value)
{
    int bool_value;
    int rc;

    if (assessment == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    switch (option) {
    case SIXEL_ASSESSMENT_OPT_ENABLE_LPIPS:
        if (parse_bool_option(value, &bool_value) != 0) {
            return SIXEL_BAD_ARGUMENT;
        }
        assessment->enable_lpips = bool_value;
        return SIXEL_OK;
    case SIXEL_ASSESSMENT_OPT_MODEL_DIR:
        if (value == NULL) {
            return SIXEL_BAD_ARGUMENT;
        }
        if (strlen(value) >= sizeof(assessment->model_dir)) {
            return SIXEL_BAD_ARGUMENT;
        }
        strcpy(assessment->model_dir, value);
        assessment->model_dir_state = 1;
        assessment->lpips_models_ready = 0;
        return SIXEL_OK;
    case SIXEL_ASSESSMENT_OPT_EXEC_PATH:
        rc = assessment_resolve_executable_dir(value,
                                              assessment->binary_dir,
                                              sizeof(assessment->binary_dir));
        if (rc != 0) {
            assessment->binary_dir_state = -1;
            assessment->binary_dir[0] = '\0';
            return SIXEL_RUNTIME_ERROR;
        }
        assessment->binary_dir_state = 1;
        assessment->lpips_models_ready = 0;
        return SIXEL_OK;
    default:
        break;
    }
    return SIXEL_BAD_ARGUMENT;
}

SIXELSTATUS
sixel_assessment_analyze(sixel_assessment_t *assessment,
                        sixel_frame_t *reference,
                        sixel_frame_t *output)
{
    Metrics metrics;
    Image ref_img;
    Image out_img;
    SIXELSTATUS status;
    int bail;

    if (assessment == NULL || reference == NULL || output == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    ref_img.width = 0;
    ref_img.height = 0;
    ref_img.channels = 0;
    ref_img.pixels = NULL;
    out_img = ref_img;

    assessment->last_error = SIXEL_OK;
    assessment->error_message[0] = '\0';
    assessment->results_ready = 0;
    g_assessment_context = assessment;
    bail = setjmp(assessment->bailout);
    if (bail != 0) {
        status = assessment->last_error;
        goto cleanup;
    }

    status = image_from_frame(reference, &ref_img);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }
    status = image_from_frame(output, &out_img);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }

    align_images(&ref_img, &out_img);

    metrics = evaluate_metrics(&ref_img, &out_img);
    metrics.lpips_vgg = compute_lpips_vgg(assessment,
                                          &ref_img,
                                          &out_img);
    store_metrics(assessment, &metrics);
    assessment->results_ready = 1;
    status = SIXEL_OK;

cleanup:
    image_free(&ref_img);
    image_free(&out_img);
    g_assessment_context = NULL;
    return status;
}

SIXELSTATUS
sixel_assessment_get_json(sixel_assessment_t *assessment,
                         sixel_assessment_json_callback_t callback,
                         void *user_data)
{
    size_t i;
    char line[128];
    int written;
    int last;
    double value;
    int index;

    if (assessment == NULL || callback == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    if (!assessment->results_ready) {
        return SIXEL_RUNTIME_ERROR;
    }

    callback("{\n", 2, user_data);
    for (i = 0; i < sizeof(g_metric_table) / sizeof(g_metric_table[0]); ++i) {
        last = (i + 1 == sizeof(g_metric_table) /
                sizeof(g_metric_table[0]));
        index = SIXEL_ASSESSMENT_INDEX(g_metric_table[i].id);
        value = assessment->results[index];
        if (isnan(value)) {
            written = snprintf(line,
                               sizeof(line),
                               "  \"%s\": NaN%s\n",
                               g_metric_table[i].json_key,
                               last ? "" : ",");
        } else {
            written = snprintf(line,
                               sizeof(line),
                               "  \"%s\": %.6f%s\n",
                               g_metric_table[i].json_key,
                               value,
                               last ? "" : ",");
        }
        if (written < 0 || (size_t)written >= sizeof(line)) {
            return SIXEL_RUNTIME_ERROR;
        }
        callback(line, (size_t)written, user_data);
    }
    callback("}\n", 2, user_data);
    return SIXEL_OK;
}
