/*
 * Verify builtin loader falls back from indexed PNG when reqcolors is lower
 * than the source palette size.
 */

#include "tests/loader/pixelformat_test_common.h"
#include "src/threading.h"

#include <stdint.h>
#include <string.h>

#define SUBOPTION_PARALLEL_ITERATIONS 64

typedef struct loader_digest_context {
    uint64_t hash;
    int frame_count;
    SIXELSTATUS status;
} loader_digest_context_t;

typedef struct loader_parallel_job {
    char image_path[PATH_MAX];
    char const *order;
    uint64_t expected_hash;
    int expected_frame_count;
    int iterations;
    struct loader_parallel_start_sync *start_sync;
    SIXELSTATUS status;
    int mismatch_iteration;
    uint64_t actual_hash;
    int actual_frame_count;
} loader_parallel_job_t;

typedef struct loader_parallel_start_sync {
    sixel_mutex_t mutex;
    sixel_cond_t cond;
    int started;
} loader_parallel_start_sync_t;

static uint64_t
hash_bytes(uint64_t hash, void const *data, size_t length)
{
    size_t index;
    unsigned char const *bytes;

    index = 0u;
    bytes = (unsigned char const *)data;
    while (index < length) {
        hash ^= (uint64_t)bytes[index];
        hash *= 1099511628211ULL;
        ++index;
    }

    return hash;
}

static uint64_t
hash_u32(uint64_t hash, unsigned int value)
{
    unsigned char bytes[4];

    bytes[0] = (unsigned char)(value & 0xffu);
    bytes[1] = (unsigned char)((value >> 8) & 0xffu);
    bytes[2] = (unsigned char)((value >> 16) & 0xffu);
    bytes[3] = (unsigned char)((value >> 24) & 0xffu);
    return hash_bytes(hash, bytes, sizeof(bytes));
}

static SIXELSTATUS
append_frame_digest(sixel_frame_t *frame, uint64_t *hash)
{
    size_t max_size;
    int width;
    int height;
    int pixelformat;
    int ncolors;
    int depth;
    size_t pixel_count;
    size_t pixel_bytes;
    unsigned char *pixels;
    unsigned char *palette;

    width = 0;
    height = 0;
    pixelformat = 0;
    ncolors = 0;
    depth = 0;
    max_size = (size_t)-1;
    pixel_count = 0u;
    pixel_bytes = 0u;
    pixels = NULL;
    palette = NULL;
    if (frame == NULL || hash == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    width = sixel_frame_get_width(frame);
    height = sixel_frame_get_height(frame);
    pixelformat = sixel_frame_get_pixelformat(frame);
    ncolors = sixel_frame_get_ncolors(frame);
    depth = sixel_helper_compute_depth(pixelformat);
    if (width <= 0 || height <= 0 || depth <= 0) {
        return SIXEL_BAD_ARGUMENT;
    }

    if ((size_t)width > max_size / (size_t)height) {
        return SIXEL_BAD_ARGUMENT;
    }
    pixel_count = (size_t)width * (size_t)height;
    if ((size_t)depth > 0u && pixel_count > max_size / (size_t)depth) {
        return SIXEL_BAD_ARGUMENT;
    }
    pixel_bytes = pixel_count * (size_t)depth;

    pixels = sixel_frame_get_pixels(frame);
    if (pixels == NULL || pixel_bytes == 0u) {
        return SIXEL_BAD_ARGUMENT;
    }

    *hash = hash_u32(*hash, (unsigned int)width);
    *hash = hash_u32(*hash, (unsigned int)height);
    *hash = hash_u32(*hash, (unsigned int)pixelformat);
    *hash = hash_u32(*hash, (unsigned int)ncolors);
    *hash = hash_bytes(*hash, pixels, pixel_bytes);

    if (ncolors > 0) {
        palette = sixel_frame_get_palette(frame);
        if (palette == NULL) {
            return SIXEL_BAD_ARGUMENT;
        }
        if ((size_t)ncolors > max_size / 3u) {
            return SIXEL_BAD_ARGUMENT;
        }
        *hash = hash_bytes(*hash, palette, (size_t)ncolors * 3u);
    }

    return SIXEL_OK;
}

static SIXELSTATUS
capture_loader_digest(sixel_frame_t *frame, void *data)
{
    loader_digest_context_t *context;
    SIXELSTATUS status;

    context = (loader_digest_context_t *)data;
    status = SIXEL_OK;
    if (context == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    if (SIXEL_FAILED(context->status)) {
        return context->status;
    }

    status = append_frame_digest(frame, &context->hash);
    if (SIXEL_FAILED(status)) {
        context->status = status;
        return status;
    }

    context->frame_count += 1;
    return SIXEL_OK;
}

static SIXELSTATUS
compute_loader_digest(char const *image_path,
                      char const *order,
                      uint64_t *hash_out,
                      int *frame_count_out)
{
    SIXELSTATUS status;
    sixel_loader_t *loader;
    loader_digest_context_t context;

    status = SIXEL_FALSE;
    loader = NULL;
    context.hash = 1469598103934665603ULL;
    context.frame_count = 0;
    context.status = SIXEL_OK;
    if (image_path == NULL || order == NULL ||
        hash_out == NULL || frame_count_out == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    status = sixel_loader_new(&loader, NULL);
    if (SIXEL_FAILED(status)) {
        return status;
    }

    status = sixel_loader_setopt(loader,
                                 SIXEL_LOADER_OPTION_CONTEXT,
                                 &context);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }
    status = sixel_loader_setopt(loader,
                                 SIXEL_LOADER_OPTION_LOADER_ORDER,
                                 order);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }
    status = sixel_loader_load_file(loader, image_path, capture_loader_digest);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }
    if (SIXEL_FAILED(context.status)) {
        status = context.status;
        goto cleanup;
    }
    if (context.frame_count <= 0) {
        status = SIXEL_BAD_ARGUMENT;
        goto cleanup;
    }

    *hash_out = context.hash;
    *frame_count_out = context.frame_count;
    status = SIXEL_OK;

cleanup:
    sixel_loader_unref(loader);
    return status;
}

static char const *
resolve_source_root_for_loader_test(void)
{
#if defined(_MSC_VER)
    char *source_root_dupe;
    size_t source_root_len;

    source_root_dupe = NULL;
    source_root_len = 0u;
    _dupenv_s(&source_root_dupe, &source_root_len, "MESON_SOURCE_ROOT");
    if (source_root_dupe == NULL) {
        _dupenv_s(&source_root_dupe, &source_root_len, "abs_top_srcdir");
    }
    if (source_root_dupe == NULL) {
        _dupenv_s(&source_root_dupe, &source_root_len, "TOP_SRCDIR");
    }
    if (source_root_dupe != NULL) {
        return source_root_dupe;
    }
    return ".";
#else
    char const *source_root;

    source_root = getenv("MESON_SOURCE_ROOT");
    if (source_root == NULL) {
        source_root = getenv("abs_top_srcdir");
    }
    if (source_root == NULL) {
        source_root = getenv("TOP_SRCDIR");
    }
    if (source_root == NULL) {
        source_root = ".";
    }
    return source_root;
#endif
}

static int
loader_parallel_worker(void *arg)
{
    loader_parallel_job_t *job;
    SIXELSTATUS status;
    uint64_t hash;
    int frame_count;
    int iteration;

    job = (loader_parallel_job_t *)arg;
    status = SIXEL_OK;
    hash = 0u;
    frame_count = 0;
    iteration = 0;
    if (job == NULL || job->start_sync == NULL) {
        return 1;
    }

    sixel_mutex_lock(&job->start_sync->mutex);
    while (job->start_sync->started == 0) {
        sixel_cond_wait(&job->start_sync->cond, &job->start_sync->mutex);
    }
    sixel_mutex_unlock(&job->start_sync->mutex);

    while (iteration < job->iterations) {
        status = compute_loader_digest(job->image_path,
                                       job->order,
                                       &hash,
                                       &frame_count);
        if (SIXEL_FAILED(status)) {
            job->status = status;
            job->mismatch_iteration = iteration;
            return 1;
        }
        if (hash != job->expected_hash ||
            frame_count != job->expected_frame_count) {
            job->status = SIXEL_BAD_ARGUMENT;
            job->mismatch_iteration = iteration;
            job->actual_hash = hash;
            job->actual_frame_count = frame_count;
            return 1;
        }
        ++iteration;
    }

    job->status = SIXEL_OK;
    return 0;
}

static int
run_loader_suboption_parallel_isolation_test(void)
{
    char image_path[PATH_MAX];
    char const *source_root;
    char const *relative_images[] = {
        "/tests/data/colormgmt/input/png/idx/img_idx_icc0_srgb0_chrm0_gama1.png",
        "/tests/data/colormgmt/input/png/rgb/img_rgb_icc0_srgb0_chrm0_gama1.png",
        "/tests/data/colormgmt/input/png/gray/img_gray_icc0_srgb0_chrm0_gama1.png",
    };
    char const *order_zero;
    char const *order_one;
    size_t image_index;
    SIXELSTATUS status;
    uint64_t baseline_zero;
    uint64_t baseline_one;
    int frames_zero;
    int frames_one;
    int found_sensitive_case;
    loader_parallel_start_sync_t start_sync;
    loader_parallel_job_t job_zero;
    loader_parallel_job_t job_one;
    sixel_thread_t thread_zero;
    sixel_thread_t thread_one;
    int thread_zero_started;
    int thread_one_started;
    int create_status;

    source_root = NULL;
    order_zero = "builtin:cms=0!";
    order_one = "builtin:cms=1!";
    image_index = 0u;
    status = SIXEL_OK;
    baseline_zero = 0u;
    baseline_one = 0u;
    frames_zero = 0;
    frames_one = 0;
    found_sensitive_case = 0;
    memset(&start_sync, 0, sizeof(start_sync));
    memset(&job_zero, 0, sizeof(job_zero));
    memset(&job_one, 0, sizeof(job_one));
    memset(&thread_zero, 0, sizeof(thread_zero));
    memset(&thread_one, 0, sizeof(thread_one));
    thread_zero_started = 0;
    thread_one_started = 0;
    create_status = SIXEL_OK;

    source_root = resolve_source_root_for_loader_test();
    while (image_index < sizeof(relative_images) / sizeof(relative_images[0])) {
        if (build_image_path(source_root,
                             relative_images[image_index],
                             image_path,
                             sizeof(image_path)) != 0) {
            ++image_index;
            continue;
        }
        status = compute_loader_digest(image_path,
                                       order_zero,
                                       &baseline_zero,
                                       &frames_zero);
        if (SIXEL_FAILED(status)) {
            ++image_index;
            continue;
        }
        status = compute_loader_digest(image_path,
                                       order_one,
                                       &baseline_one,
                                       &frames_one);
        if (SIXEL_FAILED(status)) {
            ++image_index;
            continue;
        }
        if (baseline_zero != baseline_one || frames_zero != frames_one) {
            found_sensitive_case = 1;
            break;
        }
        ++image_index;
    }

    if (!found_sensitive_case) {
        fprintf(stderr,
                "parallel loader suboption isolation: no sensitive sample found\n");
        return SIXEL_TEST_SKIP;
    }

    if (sixel_mutex_init(&start_sync.mutex) != 0) {
        fprintf(stderr,
                "parallel loader suboption isolation: mutex unavailable\n");
        return SIXEL_TEST_SKIP;
    }
    if (sixel_cond_init(&start_sync.cond) != 0) {
        sixel_mutex_destroy(&start_sync.mutex);
        fprintf(stderr,
                "parallel loader suboption isolation: condition variable unavailable\n");
        return SIXEL_TEST_SKIP;
    }
    start_sync.started = 0;

    memcpy(job_zero.image_path, image_path, strlen(image_path) + 1u);
    memcpy(job_one.image_path, image_path, strlen(image_path) + 1u);
    job_zero.order = order_zero;
    job_one.order = order_one;
    job_zero.expected_hash = baseline_zero;
    job_zero.expected_frame_count = frames_zero;
    job_one.expected_hash = baseline_one;
    job_one.expected_frame_count = frames_one;
    job_zero.iterations = SUBOPTION_PARALLEL_ITERATIONS;
    job_one.iterations = SUBOPTION_PARALLEL_ITERATIONS;
    job_zero.start_sync = &start_sync;
    job_one.start_sync = &start_sync;
    job_zero.status = SIXEL_OK;
    job_one.status = SIXEL_OK;
    job_zero.mismatch_iteration = -1;
    job_one.mismatch_iteration = -1;

    create_status = sixel_thread_create(&thread_zero,
                                        loader_parallel_worker,
                                        &job_zero);
    if (SIXEL_FAILED(create_status)) {
        fprintf(stderr,
                "parallel loader suboption isolation: thread creation unavailable\n");
        sixel_cond_destroy(&start_sync.cond);
        sixel_mutex_destroy(&start_sync.mutex);
        return SIXEL_TEST_SKIP;
    }
    thread_zero_started = 1;
    create_status = sixel_thread_create(&thread_one,
                                        loader_parallel_worker,
                                        &job_one);
    if (SIXEL_FAILED(create_status)) {
        fprintf(stderr,
                "parallel loader suboption isolation: second thread creation unavailable\n");
        sixel_mutex_lock(&start_sync.mutex);
        start_sync.started = 1;
        sixel_cond_broadcast(&start_sync.cond);
        sixel_mutex_unlock(&start_sync.mutex);
        sixel_thread_join(&thread_zero);
        sixel_cond_destroy(&start_sync.cond);
        sixel_mutex_destroy(&start_sync.mutex);
        return SIXEL_TEST_SKIP;
    }
    thread_one_started = 1;

    sixel_mutex_lock(&start_sync.mutex);
    start_sync.started = 1;
    sixel_cond_broadcast(&start_sync.cond);
    sixel_mutex_unlock(&start_sync.mutex);
    if (thread_zero_started) {
        sixel_thread_join(&thread_zero);
    }
    if (thread_one_started) {
        sixel_thread_join(&thread_one);
    }

    if (SIXEL_FAILED(job_zero.status)) {
        fprintf(stderr,
                "parallel loader suboption isolation mismatch (%s iter=%d "
                "expected_hash=%llu actual_hash=%llu expected_frames=%d "
                "actual_frames=%d)\n",
                job_zero.order,
                job_zero.mismatch_iteration,
                (unsigned long long)job_zero.expected_hash,
                (unsigned long long)job_zero.actual_hash,
                job_zero.expected_frame_count,
                job_zero.actual_frame_count);
        sixel_cond_destroy(&start_sync.cond);
        sixel_mutex_destroy(&start_sync.mutex);
        return 1;
    }
    if (SIXEL_FAILED(job_one.status)) {
        fprintf(stderr,
                "parallel loader suboption isolation mismatch (%s iter=%d "
                "expected_hash=%llu actual_hash=%llu expected_frames=%d "
                "actual_frames=%d)\n",
                job_one.order,
                job_one.mismatch_iteration,
                (unsigned long long)job_one.expected_hash,
                (unsigned long long)job_one.actual_hash,
                job_one.expected_frame_count,
                job_one.actual_frame_count);
        sixel_cond_destroy(&start_sync.cond);
        sixel_mutex_destroy(&start_sync.mutex);
        return 1;
    }

    sixel_cond_destroy(&start_sync.cond);
    sixel_mutex_destroy(&start_sync.mutex);
    return 0;
}

static SIXELSTATUS
new_builtin_component_for_reqcolors_fallback_test(sixel_allocator_t *allocator,
                                                  sixel_loader_component_t **ppcomponent)
{
    return create_loader_component_by_name("builtin", allocator, ppcomponent);
}

int
test_loader_0021_loader_builtin_indexed_png_reqcolors_fallback(int argc,
                                                                char **argv)
{
    int result;

    (void)argc;
    (void)argv;

    result = run_loader_component_case_with_options(
        "builtin loader indexed png reqcolors fallback",
        "/tests/data/inputs/formats/snake-png-pal8.png",
        SIXEL_PIXELFORMAT_RGB888,
        64,
        64,
        1,
        1,
        253,
        new_builtin_component_for_reqcolors_fallback_test);
    if (result != 0) {
        return result;
    }

    result = run_loader_suboption_parallel_isolation_test();
    if (result == SIXEL_TEST_SKIP) {
        return 0;
    }

    return result;
}

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
