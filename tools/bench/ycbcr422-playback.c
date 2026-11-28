/* SPDX-License-Identifier: MIT */
/* Compare consecutive 1080p playback submissions on Linux.
 * Build against a configured shared library:
 * cc -O2 -Wall -Wextra -pthread -I"$build/include" \
 *   tools/bench/ycbcr422-playback.c -L"$build/src/.libs" -lsixel \
 *   -Wl,-rpath,"$build/src/.libs" -o ycbcr422-playback
 * Run each route in a fresh process with SIXEL_FRAME_YCBCR_CHROMA=linear:
 * ./ycbcr422-playback packed ahead 12
 * ./ycbcr422-playback rgb ahead 12
 * RGB uses the same public scalar conversion before submitting a frame.
 * Both routes hold capacity for two frames and warm up two frames. Timing
 * covers a fresh session from attach to drain. The custom allocator counts
 * malloc, calloc and realloc successes and the high-water mark above the
 * warmup baseline.
 * RSS also includes the common input fixture and allocations outside the
 * custom allocator. The output hash checks that both routes did the same work.
 */
#define _POSIX_C_SOURCE 200809L
#include <inttypes.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/resource.h>
#include <time.h>
#include <sixel.h>

enum { WIDTH = 1920, HEIGHT = 1080, STRIDE = WIDTH * 2 + 64, WARMUP = 2 };

typedef union allocation_header {
    long double alignment;
    void *pointer_alignment;
    size_t size;
} allocation_header_t;

static pthread_mutex_t allocation_lock = PTHREAD_MUTEX_INITIALIZER;
static size_t live_bytes, peak_bytes, allocation_calls;

static void
record_allocation(size_t old_size, size_t new_size)
{
    pthread_mutex_lock(&allocation_lock);
    live_bytes -= old_size;
    live_bytes += new_size;
    if (live_bytes > peak_bytes) {
        peak_bytes = live_bytes;
    }
    ++allocation_calls;
    pthread_mutex_unlock(&allocation_lock);
}

static void *
bench_malloc(size_t size)
{
    allocation_header_t *block;

    if (size > SIZE_MAX - sizeof(*block)) {
        return NULL;
    }
    block = malloc(sizeof(*block) + size);
    if (block == NULL) {
        return NULL;
    }
    block->size = size;
    record_allocation(0, size);
    return block + 1;
}

static void *
bench_calloc(size_t count, size_t size)
{
    allocation_header_t *block;
    size_t bytes;

    if (size != 0 && count > (SIZE_MAX - sizeof(*block)) / size) {
        return NULL;
    }
    bytes = count * size;
    block = calloc(1, sizeof(*block) + bytes);
    if (block == NULL) {
        return NULL;
    }
    block->size = bytes;
    record_allocation(0, bytes);
    return block + 1;
}

static void *
bench_realloc(void *pointer, size_t size)
{
    allocation_header_t *block;
    size_t old_size;

    if (size > SIZE_MAX - sizeof(*block)) {
        return NULL;
    }
    if (pointer == NULL) {
        return bench_malloc(size);
    }
    block = (allocation_header_t *)pointer - 1;
    old_size = block->size;
    block = realloc(block, sizeof(*block) + size);
    if (block == NULL) {
        return NULL;
    }
    block->size = size;
    record_allocation(old_size, size);
    return block + 1;
}

static void
bench_free(void *pointer)
{
    allocation_header_t *block;

    if (pointer != NULL) {
        block = (allocation_header_t *)pointer - 1;
        pthread_mutex_lock(&allocation_lock);
        live_bytes -= block->size;
        pthread_mutex_unlock(&allocation_lock);
        free(block);
    }
}

typedef struct observation {
    size_t bytes;
    uint64_t hash;
    int completed;
    SIXELSTATUS failure;
} observation_t;

static int
write_hash(char *bytes, int size, void *context)
{
    observation_t *obs = context;
    int i;

    for (i = 0; i < size; ++i) {
        obs->hash ^= (unsigned char)bytes[i];
        obs->hash *= UINT64_C(1099511628211);
    }
    obs->bytes += (size_t)size;
    return size;
}

static void
complete_frame(sixel_playback_result_t const *result, void *context)
{
    observation_t *obs = context;

    ++obs->completed;
    if (result->status != SIXEL_OK && obs->failure == SIXEL_OK) {
        obs->failure = result->status;
    }
}

/* Moving bars over luma ramps with both chroma channels varying sideways.
 * Allocate and fill all identical source frames before starting the clock. */
static void
fill_frame(unsigned char *bytes, int frame)
{
    int x, y, position, bar;

    for (y = 0; y < HEIGHT; ++y) {
        for (x = 0; x < WIDTH; x += 2) {
            position = (x + 22 * frame) % WIDTH;
            bar = position / 240;
            bytes[2 * x] = (unsigned char)(36 + (y / 9 + 9 * bar) % 165);
            bytes[2 * x + 1] = (unsigned char)(100 + 7 * bar);
            bytes[2 * x + 2] = (unsigned char)(bytes[2 * x] + (x / 64 & 1));
            bytes[2 * x + 3] = (unsigned char)(151 - 6 * bar);
        }
        bytes += STRIDE;
    }
}

static double
seconds(void)
{
    struct timespec now;

    clock_gettime(CLOCK_MONOTONIC, &now);
    return now.tv_sec + now.tv_nsec * 1e-9;
}

static SIXELSTATUS
submit_frame(sixel_playback_t *playback, sixel_allocator_t *allocator,
             sixel_ycbcr_def_t *definition, unsigned char *pixels,
             int index, int prepare_rgb, double *prepare_seconds)
{
    sixel_playback_ticket_t ticket;
    sixel_ycbcr_frame_view_t view = {sizeof(view), SIXEL_PIXELFORMAT_YUYV,
        WIDTH, HEIGHT, 1, {{pixels, (size_t)STRIDE * HEIGHT, 0, STRIDE}},
        definition};
    sixel_frame_t *frame = NULL;
    SIXELSTATUS status;
    double start;

    status = sixel_playback_reserve(playback, SIXEL_PLAYBACK_WAIT, -1, &ticket);
    if (status != SIXEL_OK) {
        return status;
    }
    start = seconds();
    status = sixel_frame_new(&frame, allocator);
    if (status == SIXEL_OK) {
        status = sixel_frame_init_ycbcr_borrowed(frame, &view);
    }
    if (status == SIXEL_OK && prepare_rgb) {
        status = sixel_frame_set_pixelformat(frame,
                                              SIXEL_PIXELFORMAT_RGBFLOAT32);
    }
    *prepare_seconds += seconds() - start;
    if (status == SIXEL_OK) {
        sixel_frame_set_frame_no(frame, index);
        sixel_frame_set_multiframe(frame, 1);
        status = sixel_playback_submit_reserved(playback, &ticket, frame, NULL);
    } else {
        sixel_playback_release_reservation(playback, &ticket);
    }
    sixel_frame_unref(frame);
    return status;
}

int
main(int argc, char **argv)
{
    sixel_allocator_t *allocator = NULL;
    sixel_encoder_t *encoder = NULL;
    sixel_ycbcr_def_t *definition = NULL;
    sixel_ycbcr_def_config_t color = {sizeof(color), SIXEL_YCBCR_ENCODING_BT709,
        SIXEL_YCBCR_RANGE_FULL, 1, 0, NULL};
    sixel_color_def_t *rgb = NULL;
    sixel_output_t *output = NULL;
    sixel_playback_t *playback = NULL;
    sixel_playback_config_t config;
    struct rusage usage;
    observation_t obs = {0};
    unsigned char *input = NULL;
    char *end;
    long count;
    int prepare_rgb, i, result = EXIT_FAILURE;
    size_t frame_bytes, baseline, peak, calls, rss_kib;
    SIXELSTATUS status;
    double start, elapsed, prepare_seconds = 0.0;

    if (argc != 4 ||
        (strcmp(argv[1], "packed") != 0 && strcmp(argv[1], "rgb") != 0) ||
        (strcmp(argv[2], "ahead") != 0 && strcmp(argv[2], "serial") != 0)) {
        fprintf(stderr, "Usage: %s packed|rgb ahead|serial FRAME_COUNT\n",
                argv[0]);
        return EXIT_FAILURE;
    }
    count = strtol(argv[3], &end, 10);
    if (*end != '\0' || count < 1 || count > 120) {
        return EXIT_FAILURE;
    }
    prepare_rgb = strcmp(argv[1], "rgb") == 0;
    frame_bytes = (size_t)STRIDE * HEIGHT;
    input = malloc(frame_bytes * (size_t)(count + WARMUP));
    if (input == NULL) {
        goto end;
    }
    for (i = 0; i < count + WARMUP; ++i) {
        fill_frame(input + (size_t)i * frame_bytes, i);
    }

    rgb = sixel_color_def_new_named("srgb");
    color.rgb = rgb;
    definition = sixel_ycbcr_def_new(&color);
    if (definition == NULL ||
        sixel_allocator_new(&allocator, bench_malloc, bench_calloc,
                            bench_realloc, bench_free) ||
        sixel_encoder_new(&encoder, allocator) ||
        sixel_encoder_setopt(encoder, SIXEL_OPTFLAG_GPU_POLICY, "off") ||
        sixel_encoder_setopt(encoder, SIXEL_OPTFLAG_OUTFILE, "/dev/null") ||
        sixel_encoder_setopt(encoder, SIXEL_OPTFLAG_COLORS, "16") ||
        sixel_encoder_setopt(encoder, SIXEL_OPTFLAG_DIFFUSION, "none") ||
        sixel_encoder_setopt(encoder, SIXEL_OPTFLAG_QUANTIZE_MODEL,
                             "heckbert") ||
        sixel_encoder_setopt(encoder, SIXEL_OPTFLAG_WORKING_COLORSPACE,
                             "srgb") ||
        sixel_encoder_setopt(encoder, SIXEL_OPTFLAG_CLUSTERING_COLORSPACE,
                             "srgb") ||
        sixel_encoder_setopt(encoder, SIXEL_OPTFLAG_THREADING_POLICY,
            strcmp(argv[2], "ahead") == 0 ? "greedy:B2:prepare=ahead" :
                                            "greedy:B2:prepare=serial") ||
        sixel_output_new(&output, write_hash, &obs, allocator) ||
        sixel_playback_config_init(&config, sizeof(config))) {
        goto end;
    }
    config.capacity = 2;
    config.done = complete_frame;
    config.done_context = &obs;
    if (sixel_playback_new(&playback, encoder, output, &config, allocator)) {
        goto end;
    }
    for (i = 0; i < WARMUP; ++i) {
        status = submit_frame(playback, allocator, definition,
            input + (size_t)i * frame_bytes, i, prepare_rgb, &prepare_seconds);
        if (status != SIXEL_OK) {
            goto end;
        }
    }
    if (sixel_playback_drain(playback, -1) ||
        obs.failure != SIXEL_OK || obs.completed != WARMUP) {
        goto end;
    }

    /* Completion can precede slot destruction. Detach and join the warmup
     * worker before taking the baseline, then include the new session's
     * allocations so worker startup cannot race the baseline snapshot. */
    sixel_playback_unref(playback);
    playback = NULL;
    pthread_mutex_lock(&allocation_lock);
    baseline = live_bytes;
    peak_bytes = baseline;
    allocation_calls = 0;
    pthread_mutex_unlock(&allocation_lock);
    memset(&obs, 0, sizeof(obs));
    obs.hash = UINT64_C(14695981039346656037);
    prepare_seconds = 0.0;
    start = seconds();
    if (sixel_playback_new(&playback, encoder, output, &config, allocator)) {
        goto end;
    }
    for (; i < count + WARMUP; ++i) {
        status = submit_frame(playback, allocator, definition,
            input + (size_t)i * frame_bytes, i, prepare_rgb, &prepare_seconds);
        if (status != SIXEL_OK) {
            goto end;
        }
    }
    if (sixel_playback_drain(playback, -1) ||
        obs.failure != SIXEL_OK || obs.completed != count || obs.bytes == 0) {
        goto end;
    }
    elapsed = seconds() - start;
    pthread_mutex_lock(&allocation_lock);
    peak = peak_bytes - baseline;
    calls = allocation_calls;
    pthread_mutex_unlock(&allocation_lock);
    if (getrusage(RUSAGE_SELF, &usage) != 0) {
        goto end;
    }
    rss_kib = (size_t)usage.ru_maxrss;
    puts("route,prepare,frames,elapsed_ms,caller_prepare_ms,"
         "allocator_calls,temp_peak_bytes,peak_rss_kib,"
         "output_bytes,output_hash");
    printf("%s,%s,%ld,%.3f,%.3f,%zu,%zu,%zu,%zu,%016" PRIx64 "\n",
        argv[1], argv[2], count, elapsed * 1000.0, prepare_seconds * 1000.0,
        calls, peak, rss_kib, obs.bytes, obs.hash);
    result = EXIT_SUCCESS;
end:
    if (result != EXIT_SUCCESS) {
        fprintf(stderr, "Benchmark failed: %s\n",
                sixel_helper_get_additional_message());
    }
    sixel_playback_unref(playback);
    sixel_output_unref(output);
    sixel_encoder_unref(encoder);
    sixel_allocator_unref(allocator);
    sixel_ycbcr_def_free(definition);
    sixel_color_def_free(rgb);
    free(input);
    return result;
}
