/*
 * lsqa.c - Thin CLI wrapper around the libsixel assessment engine.
 *
 *  Pipeline overview:
 *
 *        +---------------+    +------------------+    +-----------------+
 *        |  Frame input  | -> |  Assessment API  | -> |  JSON emission  |
 *        +---------------+    +------------------+    +-----------------+
 *             |                         |                        |
 *             |                         |                        +--> stdout
 *             |                         +--> sixel_assessment_t   +--> files
 *             +--> libsixel loader      +--> ONNX LPIPS bridge
 */

#define _POSIX_C_SOURCE 200809L
#define _XOPEN_SOURCE 700

#include "config.h"

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sixel.h>

#if defined(_WIN32)
#include <io.h>
#include <windows.h>
#else
#include <unistd.h>
#endif

#if !defined(PATH_MAX)
#define PATH_MAX 4096
#endif

#if defined(_WIN32)
#define SIXEL_PATH_SEP '\\'
#else
#define SIXEL_PATH_SEP '/'
#endif

typedef struct LoaderCapture {
    sixel_frame_t *frame;
} LoaderCapture;

static SIXELSTATUS
capture_first_frame(sixel_frame_t *frame, void *context)
{
    LoaderCapture *capture;

    capture = (LoaderCapture *)context;
    if (capture->frame == NULL) {
        sixel_frame_ref(frame);
        capture->frame = frame;
    }
    return SIXEL_OK;
}

static int
load_frame(char const *path, sixel_allocator_t *allocator,
           sixel_frame_t **out_frame)
{
    LoaderCapture capture;
    SIXELSTATUS status;

    capture.frame = NULL;
    status = sixel_helper_load_image_file(path,
                                          1,
                                          0,
                                          SIXEL_PALETTE_MAX,
                                          NULL,
                                          SIXEL_LOOP_DISABLE,
                                          capture_first_frame,
                                          0,
                                          NULL,
                                          NULL,
                                          &capture,
                                          allocator);
    if (SIXEL_FAILED(status) || capture.frame == NULL) {
        fprintf(stderr,
                "libsixel loader failed for %s: %s\n",
                path,
                sixel_helper_format_error(status));
        if (capture.frame != NULL) {
            sixel_frame_unref(capture.frame);
        }
        return -1;
    }
    *out_frame = capture.frame;
    return 0;
}

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

typedef struct MetricBinding {
    const char *name;
    size_t offset;
} MetricBinding;

static const MetricBinding g_metric_bindings[] = {
    {"MS-SSIM", offsetof(Metrics, ms_ssim)},
    {"HighFreqRatio_out", offsetof(Metrics, high_freq_out)},
    {"HighFreqRatio_ref", offsetof(Metrics, high_freq_ref)},
    {"HighFreqRatio_delta", offsetof(Metrics, high_freq_delta)},
    {"StripeScore_ref", offsetof(Metrics, stripe_ref)},
    {"StripeScore_out", offsetof(Metrics, stripe_out)},
    {"StripeScore_rel", offsetof(Metrics, stripe_rel)},
    {"BandingIndex_rel", offsetof(Metrics, band_run_rel)},
    {"BandingIndex_grad_rel", offsetof(Metrics, band_grad_rel)},
    {"ClipRate_L_ref", offsetof(Metrics, clip_l_ref)},
    {"ClipRate_R_ref", offsetof(Metrics, clip_r_ref)},
    {"ClipRate_G_ref", offsetof(Metrics, clip_g_ref)},
    {"ClipRate_B_ref", offsetof(Metrics, clip_b_ref)},
    {"ClipRate_L_out", offsetof(Metrics, clip_l_out)},
    {"ClipRate_R_out", offsetof(Metrics, clip_r_out)},
    {"ClipRate_G_out", offsetof(Metrics, clip_g_out)},
    {"ClipRate_B_out", offsetof(Metrics, clip_b_out)},
    {"ClipRate_L_rel", offsetof(Metrics, clip_l_rel)},
    {"ClipRate_R_rel", offsetof(Metrics, clip_r_rel)},
    {"ClipRate_G_rel", offsetof(Metrics, clip_g_rel)},
    {"ClipRate_B_rel", offsetof(Metrics, clip_b_rel)},
    {"Δ Chroma_mean", offsetof(Metrics, delta_chroma_mean)},
    {"Δ E00_mean", offsetof(Metrics, delta_e00_mean)},
    {"GMSD", offsetof(Metrics, gmsd_value)},
    {"PSNR_Y", offsetof(Metrics, psnr_y)},
    {"LPIPS(vgg)", offsetof(Metrics, lpips_vgg)},
};

static void
metrics_init(Metrics *metrics)
{
    size_t i;
    unsigned char *base;

    base = (unsigned char *)metrics;
    for (i = 0; i < sizeof(g_metric_bindings) / sizeof(g_metric_bindings[0]); ++i) {
        float *field;

        field = (float *)(void *)(base + g_metric_bindings[i].offset);
        *field = NAN;
    }
}

static void
metrics_set_value(Metrics *metrics, const char *name, double value, int is_nan)
{
    size_t i;
    unsigned char *base;

    base = (unsigned char *)metrics;
    for (i = 0; i < sizeof(g_metric_bindings) / sizeof(g_metric_bindings[0]); ++i) {
        if (strcmp(name, g_metric_bindings[i].name) == 0) {
            float *field;

            field = (float *)(void *)(base + g_metric_bindings[i].offset);
            if (is_nan) {
                *field = NAN;
            } else {
                *field = (float)value;
            }
            return;
        }
    }
}

typedef struct MetricItem {
    const char *name;
    float value;
} MetricItem;

static void
verbose_print(const Metrics *m)
{
    MetricItem items[] = {
        {"MS-SSIM", m->ms_ssim},
        {"HighFreqRatio_out", m->high_freq_out},
        {"HighFreqRatio_ref", m->high_freq_ref},
        {"HighFreqRatio_delta", m->high_freq_delta},
        {"StripeScore_ref", m->stripe_ref},
        {"StripeScore_out", m->stripe_out},
        {"StripeScore_rel", m->stripe_rel},
        {"BandingIndex_rel", m->band_run_rel},
        {"BandingIndex_grad_rel", m->band_grad_rel},
        {"ClipRate_L_ref", m->clip_l_ref},
        {"ClipRate_R_ref", m->clip_r_ref},
        {"ClipRate_G_ref", m->clip_g_ref},
        {"ClipRate_B_ref", m->clip_b_ref},
        {"ClipRate_L_out", m->clip_l_out},
        {"ClipRate_R_out", m->clip_r_out},
        {"ClipRate_G_out", m->clip_g_out},
        {"ClipRate_B_out", m->clip_b_out},
        {"ClipRate_L_rel", m->clip_l_rel},
        {"ClipRate_R_rel", m->clip_r_rel},
        {"ClipRate_G_rel", m->clip_g_rel},
        {"ClipRate_B_rel", m->clip_b_rel},
        {"Δ Chroma_mean", m->delta_chroma_mean},
        {"Δ E00_mean", m->delta_e00_mean},
        {"GMSD", m->gmsd_value},
        {"PSNR_Y", m->psnr_y},
        {"LPIPS(vgg)", m->lpips_vgg},
    };
    size_t i;

    fprintf(stderr, "\n=== Image Quality Report (Raw Metrics) ===\n");
    for (i = 0; i < sizeof(items) / sizeof(items[0]); ++i) {
        if (isnan(items[i].value)) {
            fprintf(stderr, "%24s: NaN\n", items[i].name);
        } else {
            fprintf(stderr, "%24s: %.6f\n", items[i].name, items[i].value);
        }
    }
}

typedef struct JsonCollector {
    FILE *stream;
    Metrics *metrics;
} JsonCollector;

static void
json_collect_callback(char const *chunk, size_t length, void *user_data)
{
    JsonCollector *collector;
    char buffer[256];
    char *start;
    char *end;
    char *colon;
    char name[64];
    double value;
    int is_nan;

    collector = (JsonCollector *)user_data;
    if (collector->stream != NULL) {
        if (fwrite(chunk, 1, length, collector->stream) != length) {
            fprintf(stderr, "Failed to write JSON output: %s\n",
                    strerror(errno));
        }
    }
    if (collector->metrics == NULL) {
        return;
    }
    if (length >= sizeof(buffer)) {
        length = sizeof(buffer) - 1u;
    }
    memcpy(buffer, chunk, length);
    buffer[length] = '\0';
    start = strchr(buffer, '"');
    if (start == NULL) {
        return;
    }
    end = strchr(start + 1, '"');
    if (end == NULL) {
        return;
    }
    colon = strchr(end, ':');
    if (colon == NULL) {
        return;
    }
    colon += 1;
    while (*colon == ' ') {
        colon += 1;
    }
    if (strncmp(colon, "NaN", 3) == 0) {
        is_nan = 1;
        value = NAN;
    } else {
        char *tail;

        is_nan = 0;
        value = strtod(colon, &tail);
        if (tail == colon) {
            return;
        }
    }
    if ((size_t)(end - start - 1) >= sizeof(name)) {
        return;
    }
    memcpy(name, start + 1, (size_t)(end - start - 1));
    name[end - start - 1] = '\0';
    metrics_set_value(collector->metrics, name, value, is_nan);
}

typedef struct Options {
    const char *ref_path;
    const char *out_path;
    const char *prefix;
    char prefix_buffer[PATH_MAX];
    int verbose;
} Options;

static void
print_usage(const char *prog)
{
    fprintf(stderr, "Usage: %s <reference> [output]\n", prog);
    fprintf(stderr, "       %s <reference> < output\n", prog);
    fprintf(stderr, "       %s <reference> - < output\n", prog);
}

static int
parse_args(int argc, char **argv, Options *opts)
{
    const char *ref_arg;
    const char *out_arg;
    const char *base_start;
    const char *dot_pos;
    const char *slash_pos;
#if defined(_WIN32)
    const char *alt_slash_pos;
#endif
    const char *verbose_env;
    const char *prefix_env;
    size_t prefix_len;
    int status;

    opts->ref_path = NULL;
    opts->out_path = "-";
    opts->prefix = "report";
    opts->verbose = 0;
    opts->prefix_buffer[0] = '\0';

    if (argc < 2 || argc > 3) {
        fprintf(stderr, "Invalid number of arguments\n");
        return -1;
    }

    ref_arg = argv[1];
    opts->ref_path = ref_arg;

    if (argc == 3) {
        out_arg = argv[2];
        if (strcmp(out_arg, "-") == 0) {
            opts->out_path = "-";
        } else {
            opts->out_path = out_arg;
        }
    }

    base_start = opts->out_path;
    if (strcmp(opts->out_path, "-") == 0) {
        base_start = ref_arg;
    }

    slash_pos = strrchr(base_start, '/');
#if defined(_WIN32)
    alt_slash_pos = strrchr(base_start, '\\');
    if (alt_slash_pos != NULL) {
        if (slash_pos == NULL || alt_slash_pos > slash_pos) {
            slash_pos = alt_slash_pos;
        }
    }
#endif
    if (slash_pos != NULL) {
        base_start = slash_pos + 1;
    }
    dot_pos = strrchr(base_start, '.');
    if (dot_pos != NULL && dot_pos > base_start) {
        prefix_len = (size_t)(dot_pos - base_start);
    } else {
        prefix_len = strlen(base_start);
    }
    if (prefix_len >= sizeof(opts->prefix_buffer)) {
        prefix_len = sizeof(opts->prefix_buffer) - 1u;
    }
    if (prefix_len == 0) {
        status = snprintf(opts->prefix_buffer,
                          sizeof(opts->prefix_buffer),
                          "report");
        if (status < 0) {
            return -1;
        }
    } else {
        memcpy(opts->prefix_buffer, base_start, prefix_len);
        opts->prefix_buffer[prefix_len] = '\0';
    }
    opts->prefix = opts->prefix_buffer;

    prefix_env = getenv("LSQA_PREFIX");
    if (prefix_env != NULL && prefix_env[0] != '\0') {
        status = snprintf(opts->prefix_buffer,
                          sizeof(opts->prefix_buffer),
                          "%s",
                          prefix_env);
        if (status < 0) {
            return -1;
        }
        opts->prefix = opts->prefix_buffer;
    }

    verbose_env = getenv("LSQA_VERBOSE");
    if (verbose_env != NULL && verbose_env[0] != '\0') {
        opts->verbose = 1;
    }

    return 0;
}

int
main(int argc, char **argv)
{
    Options opts;
    sixel_allocator_t *allocator;
    sixel_assessment_t *assessment;
    SIXELSTATUS status;
    sixel_frame_t *ref_frame;
    sixel_frame_t *out_frame;
    Metrics metrics;
    JsonCollector collector;
    size_t path_len;
    char *json_path;
    FILE *fp;

    allocator = NULL;
    assessment = NULL;
    ref_frame = NULL;
    out_frame = NULL;
    fp = NULL;
    json_path = NULL;

    if (parse_args(argc, argv, &opts) != 0) {
        print_usage(argv[0]);
        return EXIT_FAILURE;
    }

    status = sixel_allocator_new(&allocator,
                                 malloc,
                                 calloc,
                                 realloc,
                                 free);
    if (SIXEL_FAILED(status) || allocator == NULL) {
        fprintf(stderr,
                "Failed to allocate loader: %s\n",
                sixel_helper_format_error(status));
        return EXIT_FAILURE;
    }

    if (load_frame(opts.ref_path, allocator, &ref_frame) != 0) {
        sixel_allocator_unref(allocator);
        return EXIT_FAILURE;
    }
    if (load_frame(opts.out_path, allocator, &out_frame) != 0) {
        sixel_frame_unref(ref_frame);
        sixel_allocator_unref(allocator);
        return EXIT_FAILURE;
    }

    status = sixel_assessment_new(&assessment, allocator);
    if (SIXEL_FAILED(status) || assessment == NULL) {
        fprintf(stderr,
                "Failed to create assessment object: %s\n",
                sixel_helper_format_error(status));
        sixel_frame_unref(ref_frame);
        sixel_frame_unref(out_frame);
        sixel_allocator_unref(allocator);
        return EXIT_FAILURE;
    }

    status = sixel_assessment_setopt(assessment,
                                    SIXEL_ASSESSMENT_OPT_EXEC_PATH,
                                    argv[0]);
    if (SIXEL_FAILED(status)) {
        fprintf(stderr,
                "Warning: unable to determine executable directory (%s).\n",
                sixel_helper_format_error(status));
    }

    status = sixel_assessment_setopt(assessment,
                                    SIXEL_ASSESSMENT_OPT_ENABLE_LPIPS,
                                    "yes");
    if (SIXEL_FAILED(status)) {
        fprintf(stderr,
                "Warning: unable to determine executable directory (%s).\n",
                sixel_helper_format_error(status));
    }

    status = sixel_assessment_analyze(assessment, ref_frame, out_frame);
    if (SIXEL_FAILED(status)) {
        fprintf(stderr,
                "Assessment failed: %s\n",
                sixel_helper_format_error(status));
        sixel_assessment_unref(assessment);
        sixel_frame_unref(ref_frame);
        sixel_frame_unref(out_frame);
        sixel_allocator_unref(allocator);
        return EXIT_FAILURE;
    }

    path_len = strlen(opts.prefix) + strlen("_metrics.json") + 1u;
    json_path = (char *)malloc(path_len);
    if (json_path == NULL) {
        fprintf(stderr, "Out of memory while building JSON path\n");
        sixel_assessment_unref(assessment);
        sixel_frame_unref(ref_frame);
        sixel_frame_unref(out_frame);
        sixel_allocator_unref(allocator);
        return EXIT_FAILURE;
    }
    snprintf(json_path, path_len, "%s_metrics.json", opts.prefix);

    fp = fopen(json_path, "w");
    if (fp == NULL) {
        fprintf(stderr,
                "Failed to open %s for writing: %s\n",
                json_path,
                strerror(errno));
        free(json_path);
        sixel_assessment_unref(assessment);
        sixel_frame_unref(ref_frame);
        sixel_frame_unref(out_frame);
        sixel_allocator_unref(allocator);
        return EXIT_FAILURE;
    }

    metrics_init(&metrics);
    collector.stream = fp;
    collector.metrics = &metrics;
    status = sixel_assessment_get_json(assessment,
                                      json_collect_callback,
                                      &collector);
    fclose(fp);
    if (SIXEL_FAILED(status)) {
        fprintf(stderr,
                "Failed to write JSON file: %s\n",
                sixel_helper_format_error(status));
        free(json_path);
        sixel_assessment_unref(assessment);
        sixel_frame_unref(ref_frame);
        sixel_frame_unref(out_frame);
        sixel_allocator_unref(allocator);
        return EXIT_FAILURE;
    }

    collector.stream = stdout;
    collector.metrics = NULL;
    status = sixel_assessment_get_json(assessment,
                                      json_collect_callback,
                                      &collector);
    if (SIXEL_FAILED(status)) {
        fprintf(stderr,
                "Failed to emit JSON: %s\n",
                sixel_helper_format_error(status));
        free(json_path);
        sixel_assessment_unref(assessment);
        sixel_frame_unref(ref_frame);
        sixel_frame_unref(out_frame);
        sixel_allocator_unref(allocator);
        return EXIT_FAILURE;
    }

    if (opts.verbose) {
        verbose_print(&metrics);
        fprintf(stderr, "\nWrote: %s\n", json_path);
    }

    free(json_path);
    sixel_assessment_unref(assessment);
    sixel_frame_unref(ref_frame);
    sixel_frame_unref(out_frame);
    sixel_allocator_unref(allocator);
    return EXIT_SUCCESS;
}
