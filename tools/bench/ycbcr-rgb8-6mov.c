/* SPDX-License-Identifier: MIT
 * Optional host benchmark. Point SIXMOV to a configured 6mov source tree
 * and SIXEL_BUILD to a built libsixel tree. Use the same FFmpeg as 6mov:
 *
 * cc -O3 -I"$SIXMOV" -I"$SIXMOV/src" -I"$SIXEL_BUILD/include" \
 *   $(pkg-config --cflags libavformat libavcodec libavutil \
 *                        libswscale libswresample) \
 *   tools/bench/ycbcr-rgb8-6mov.c "$SIXMOV/src/video-color.c" \
 *   "$SIXMOV/src/pixels.c" "$SIXMOV/src/cuda-copy.c" \
 *   "$SIXMOV/src/video-scale-stub.c" \
 *   -L"$SIXEL_BUILD/src/.libs" -Wl,-rpath,"$SIXEL_BUILD/src/.libs" -lsixel \
 *   $(pkg-config --libs libavformat libavcodec libavutil \
 *                      libswscale libswresample) \
 *   -lm -o /tmp/ycbcr-rgb8-6mov
 * On macOS, set DYLD_LIBRARY_PATH="$SIXEL_BUILD/src/.libs" when running:
 * /tmp/ycbcr-rgb8-6mov [samples [6mov-cpu-threads [cpu|gpu]]]
 * To benchmark gpu, link the configured 6mov's scale_objects and backend
 * libraries in place of video-scale-stub.c. Use a host frame in both cases.
 *
 * Use the production CPU conversion directly, including its FFmpeg flags,
 * context reuse, metadata resolution and final RGB8 packing. Timings
 * exclude decode, allocations during setup, and validation after each run.
 */
#include <sixel.h>
#include "decoder_ffmpeg.c"
#include "color_ffmpeg.c"

#define SOURCE_WIDTH 1920
#define SOURCE_HEIGHT 1080
#define SAMPLES_MAX 512

static void
bench_fill(AVFrame *frame, int nv12)
{
    int x, y, cb, cr;
    unsigned char *p;

    for (y = 0; y < frame->height; ++y) {
        p = frame->data[0] + y * frame->linesize[0];
        for (x = 0; x < frame->width; ++x) {
            p[x] = (unsigned char)(16 + (x / 9 + y / 7) % 220);
        }
    }
    for (y = 0; y < frame->height / 2; ++y) {
        for (x = 0; x < frame->width / 2; ++x) {
            cb = 64 + (x / 5 + y / 3) % 129;
            cr = 72 + (3 * (x / 11) + y / 13) % 113;
            p = frame->data[1] + y * frame->linesize[1] + x * (1 + nv12);
            p[0] = (unsigned char)cb;
            if (nv12) {
                p[1] = (unsigned char)cr;
            } else {
                frame->data[2][y * frame->linesize[2] + x] = (unsigned char)cr;
            }
        }
    }
}

static void
bench_to_linear(void *context, float *rgb, int width)
{
    int x;

    for (x = 0; x < width; ++x, rgb += 3) {
        convert_pixel(context, rgb[0], rgb[1], rgb[2], rgb);
    }
}

static int
bench_compare_ticks(void const *a, void const *b)
{
    uint64_t x = *(uint64_t const *)a, y = *(uint64_t const *)b;

    return (x > y) - (x < y);
}

static int
bench_route(int route, struct ffmpeg_state *state, AVFrame *av,
            sixel_ycbcr_rgb8_t *converter, sixel_frame_t *frame,
            unsigned char *dst, size_t size, size_t stride, int transfer,
            uint64_t *elapsed)
{
    uint64_t start;
    char error[256];
    int status;

    start = sixmov_now_ns();
    if (route == 0) {
        status = ffmpeg_convert(state, av, dst, error, sizeof(error));
        if (status != 0) {
            fprintf(stderr, "6mov: %s\n", error);
        }
    } else {
        status = sixel_ycbcr_rgb8_run(converter, frame, dst, size, stride,
            transfer ? bench_to_linear : NULL, &state->color);
    }
    *elapsed = sixmov_now_ns() - start;
    return status;
}

static int
bench_case(AVFrame *av, sixel_frame_t *frame, int width, int height,
           int samples, int threads, int gpu, int nv12, int transfer)
{
    struct ffmpeg_state *state = NULL;
    sixel_ycbcr_rgb8_t *converter = NULL;
    unsigned char *dst[2] = {NULL, NULL};
    uint64_t ticks[2][SAMPLES_MAX], elapsed, error_sum = 0;
    char error[256];
    size_t stride = (size_t)width * 3, size = stride * height, i;
    size_t worst = 0;
    int iteration, first, j, route, diff, max_diff = 0, ok = 0;

    av->color_trc = transfer ? AVCOL_TRC_BT709 : AVCOL_TRC_IEC61966_2_1;
    state = calloc(1, sizeof(*state));
    dst[0] = malloc(size);
    dst[1] = malloc(size);
    if (!state || !dst[0] || !dst[1]) {
        goto end;
    }
    state->source_format = SIXMOV_SOURCE_8BIT;
    state->scale_policy = gpu ? SIXMOV_SCALE_GPU : SIXMOV_SCALE_CPU;
    state->scale_threads = (unsigned int)threads;
    if (ffmpeg_set_output(state, width, height, error, sizeof(error)) ||
        sixel_ycbcr_rgb8_new(&converter, frame, width, height,
                            SIXEL_YCBCR_RESIZE_SIGNAL, NULL)) {
        goto end;
    }
    /* Warm both plans, then alternate first execution to balance cache
     * effects. An initial 6mov call prepares the shared transfer policy. */
    for (iteration = -4; iteration < samples; ++iteration) {
        first = iteration >= 0 && (iteration & 1);
        for (j = 0; j < 2; ++j) {
            route = first ^ j;
            if (bench_route(route, state, av, converter, frame, dst[route],
                            size, stride, transfer, &elapsed)) {
                goto end;
            }
            if (iteration >= 0) {
                ticks[route][iteration] = elapsed;
            }
        }
    }
    for (i = 0; i < size; ++i) {
        diff = abs(dst[0][i] - dst[1][i]);
        error_sum += (unsigned int)diff;
        if (diff > max_diff) {
            max_diff = diff;
            worst = i;
        }
    }
    for (route = 0; route < 2; ++route) {
        qsort(ticks[route], (size_t)samples, sizeof(uint64_t),
              bench_compare_ticks);
        printf("%s,%s,%dx%d,%s,%d,%d,%.3f,%.3f,%.5f,%d,%zu,%zu,%zu\n",
            nv12 ? "nv12" : "i420", transfer ? "bt1886" : "srgb",
            width, height, route ? "libsixel" : gpu ? "6mov-gpu" : "6mov-cpu",
            samples, route ? 1 : gpu ? 0 : (int)state->used_threads,
            (ticks[route][samples / 2] +
             ticks[route][(samples - 1) / 2]) * 0.5e-6,
            ticks[route][(95 * samples + 99) / 100 - 1] * 1e-6,
            route ? (double)error_sum / size : 0.0, route ? max_diff : 0,
            route ? worst % stride / 3 : 0, route ? worst / stride : 0,
            route ? worst % 3 : 0);
    }
    ok = 1;
end:
    sixel_ycbcr_rgb8_free(converter);
    ffmpeg_close(state);
    free(dst[0]);
    free(dst[1]);
    return ok;
}

int
main(int argc, char **argv)
{
    static int const dims[][2] = {{1920, 1080}, {960, 540}, {640, 360}};
    AVFrame *av = NULL;
    sixel_frame_t *frame = NULL;
    sixel_color_def_t *rgb = NULL;
    sixel_ycbcr_def_t *def = NULL;
    sixel_ycbcr_def_config_t cfg = {sizeof(cfg), SIXEL_YCBCR_ENCODING_BT709,
        SIXEL_YCBCR_RANGE_LIMITED, 0, 1, NULL};
    sixel_ycbcr_frame_view_t view = {sizeof(view), 0,
        SOURCE_WIDTH, SOURCE_HEIGHT, 0, {{0}}, NULL};
    int samples = argc > 1 ? atoi(argv[1]) : 32;
    int threads = argc > 2 ? atoi(argv[2]) : 1;
    int gpu = argc > 3 && strcmp(argv[3], "gpu") == 0;
    int nv12, i, transfer, rows, ok = 0;
    unsigned int p;

    if (samples < 1 || samples > SAMPLES_MAX || threads < 1 || threads > 64 ||
        argc > 4 || (argc > 3 && !gpu && strcmp(argv[3], "cpu") != 0)) {
        return EXIT_FAILURE;
    }
    rgb = sixel_color_def_new_named("srgb");
    cfg.rgb = rgb;
    if (!rgb || !(def = sixel_ycbcr_def_new(&cfg)) ||
        sixel_frame_new(&frame, NULL)) {
        goto end;
    }
    view.definition = def;
    puts("layout,eotf,size,route,samples,threads,median_ms,p95_ms,"
         "mae,max,max_x,max_y,max_channel");
    for (nv12 = 0; nv12 < 2; ++nv12) {
        av = av_frame_alloc();
        if (av == NULL) {
            goto end;
        }
        av->format = nv12 ? AV_PIX_FMT_NV12 : AV_PIX_FMT_YUV420P;
        av->width = SOURCE_WIDTH;
        av->height = SOURCE_HEIGHT;
        av->colorspace = AVCOL_SPC_BT709;
        av->color_primaries = AVCOL_PRI_BT709;
        av->color_range = AVCOL_RANGE_MPEG;
        if (av_frame_get_buffer(av, 32) != 0) {
            goto end;
        }
        bench_fill(av, nv12);
        view.pixelformat = nv12 ? SIXEL_PIXELFORMAT_NV12 :
            SIXEL_PIXELFORMAT_YUV420P;
        view.plane_count = nv12 ? 2u : 3u;
        for (p = 0; p < view.plane_count; ++p) {
            rows = p ? av->height / 2 : av->height;
            view.planes[p] = (sixel_frame_plane_view_t){av->data[p],
                (size_t)av->linesize[p] * rows, 0, av->linesize[p]};
        }
        if (sixel_frame_init_ycbcr_borrowed(frame, &view)) {
            goto end;
        }
        for (transfer = 0; transfer < 2; ++transfer) {
            for (i = 0; i < 3; ++i) {
                if (!bench_case(av, frame, dims[i][0], dims[i][1], samples,
                                threads, gpu, nv12, transfer)) {
                    goto end;
                }
            }
        }
        /* Drop the borrowed descriptor before releasing its planes. */
        sixel_frame_unref(frame);
        frame = NULL;
        av_frame_free(&av);
        if (sixel_frame_new(&frame, NULL)) {
            goto end;
        }
    }
    ok = 1;
end:
    sixel_frame_unref(frame);
    av_frame_free(&av);
    sixel_ycbcr_def_free(def);
    sixel_color_def_free(rgb);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
