/* SPDX-License-Identifier: MIT */
/*
 * Step 10: dithering on several threads.
 *
 *   step10 IN PAL [fs|bayer][,bands=N][,overlap=N]
 *
 * The picture is cut into N bands of whole six-row groups, and each thread
 * takes the next band that nobody has started. A band of fs has its own error
 * history. It starts overlap rows above its first row, to warm up, and does
 * not keep those rows. The output depends on bands and overlap, but never on
 * the number of threads, which TUTORIAL_THREADS sets (default: one per CPU).
 * The default is fs,bands=4,overlap=0.
 */
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "tutorial.h"

int const dither_keeps_tone = DITHER_KEEPS_NUMBERS;

enum { MAX_THREADS = 64 };

typedef struct job {
    image_t *out;
    image_t const *in;
    palette_t const *pal;
    double gap;             /* palette_gap(), for bayer */
    int use_bayer;
    int band_height;
    int overlap;
    int bands;
    int next_band;          /* guarded by lock */
    int failed;             /* guarded by lock */
    pthread_mutex_t lock;
} job_t;

/* The same pattern as step 2. */
static int
bayer(int x, int y)
{
    int value = 0;
    int i;

    for (i = 0; i < 3; i++) {
        value = value << 2 | ((x ^ y) >> i & 1) << 1 | (y >> i & 1);
    }
    return value;
}

static int
clamp_level(double value)
{
    return value < 0.0 ? 0 : value > 255.0 ? 255 : (int)(value + 0.5);
}

static float
clamp(float value)
{
    return value < 0.0f ? 0.0f : value > 255.0f ? 255.0f : value;
}

/* Rows first to last of step 2: every pixel stands alone. */
static void
band_bayer(job_t const *job, int first, int last)
{
    image_t const *in = job->in;
    unsigned char const *p;
    double nudge;
    int x;
    int y;

    for (y = first; y < last; y++) {
        p = in->rgb + (size_t)y * (size_t)in->width * 3;
        for (x = 0; x < in->width; x++, p += 3) {
            nudge = ((bayer(x, y) + 0.5) / 64.0 - 0.5) * job->gap;
            put_color(job->out, y * in->width + x, job->pal,
                      nearest_color(job->pal, clamp_level(p[0] + nudge),
                                    clamp_level(p[1] + nudge),
                                    clamp_level(p[2] + nudge)));
        }
    }
}

/* Rows first to last of step 3, with an error history of their own that
 * begins at row start. Rows before first are computed but not kept. */
static int
band_fs(job_t const *job, int start, int first, int last)
{
    image_t const *in = job->in;
    palette_t const *pal = job->pal;
    int w = in->width;
    size_t row_size = ((size_t)w + 2) * 3 * sizeof(float);
    float *this_row = calloc(1, row_size);
    float *next_row = calloc(1, row_size);
    float *swap;
    unsigned char const *p;
    float want;
    float error;
    int index;
    int rgb[3];
    int x;
    int y;
    int c;
    int k;

    if (this_row == NULL || next_row == NULL) {
        free(this_row);
        free(next_row);
        return 1;
    }
    for (y = start; y < last; y++) {
        p = in->rgb + (size_t)y * (size_t)w * 3;
        for (x = 0; x < w; x++, p += 3) {
            k = (x + 1) * 3;
            for (c = 0; c < 3; c++) {
                rgb[c] = (int)(clamp(p[c] + this_row[k + c]) + 0.5f);
            }
            index = nearest_color(pal, rgb[0], rgb[1], rgb[2]);
            if (y >= first) {
                put_color(job->out, y * w + x, pal, index);
            }
            for (c = 0; c < 3; c++) {
                want = clamp(p[c] + this_row[k + c]);
                error = want - (float)pal->rgb[index][c];
                this_row[k + 3 + c] += error * 7.0f / 16.0f;
                next_row[k - 3 + c] += error * 3.0f / 16.0f;
                next_row[k + c] += error * 5.0f / 16.0f;
                next_row[k + 3 + c] += error * 1.0f / 16.0f;
            }
        }
        swap = this_row;
        this_row = next_row;
        next_row = swap;
        memset(next_row, 0, row_size);
    }
    free(this_row);
    free(next_row);
    return 0;
}

static void *
worker(void *arg)
{
    job_t *job = arg;
    int height = job->in->height;
    int band;
    int first;
    int last;
    int start;
    int status;

    for (;;) {
        pthread_mutex_lock(&job->lock);
        band = job->next_band++;
        pthread_mutex_unlock(&job->lock);
        if (band >= job->bands) {
            return NULL;
        }
        first = band * job->band_height;
        last = first + job->band_height;
        last = last > height ? height : last;
        start = first - job->overlap;
        start = start < 0 ? 0 : start;
        status = 0;
        if (first >= last) {
            continue;
        }
        if (job->use_bayer) {
            band_bayer(job, first, last);
        } else {
            status = band_fs(job, start, first, last);
        }
        if (status != 0) {
            pthread_mutex_lock(&job->lock);
            job->failed = 1;
            pthread_mutex_unlock(&job->lock);
        }
    }
}

static int
parse_option(char const *option, job_t *job)
{
    size_t length;
    char *end;

    job->use_bayer = 0;
    job->bands = 4;
    job->overlap = 0;
    while (option != NULL && *option != '\0') {
        length = strcspn(option, ",");
        if (length == 2 && strncmp(option, "fs", 2) == 0) {
            job->use_bayer = 0;
        } else if (length == 5 && strncmp(option, "bayer", 5) == 0) {
            job->use_bayer = 1;
        } else if (strncmp(option, "bands=", 6) == 0) {
            job->bands = (int)strtol(option + 6, &end, 10);
            if (end != option + length || job->bands < 1) {
                return 1;
            }
        } else if (strncmp(option, "overlap=", 8) == 0) {
            job->overlap = (int)strtol(option + 8, &end, 10);
            if (end != option + length || job->overlap < 0) {
                return 1;
            }
        } else {
            return 1;
        }
        option += length;
        option += *option == ',';
    }
    return 0;
}

static int
thread_count(void)
{
    char const *setting = getenv("TUTORIAL_THREADS");
    long n = 1;

    if (setting != NULL) {
        n = atol(setting);
    } else {
#ifdef _SC_NPROCESSORS_ONLN
        n = sysconf(_SC_NPROCESSORS_ONLN);
#endif
    }
    return n < 1 ? 1 : n > MAX_THREADS ? MAX_THREADS : (int)n;
}

int
dither(image_t *out, image_t const *in, palette_t const *pal,
       char const *option)
{
    pthread_t threads[MAX_THREADS];
    job_t job;
    int wanted;
    int started;
    int i;

    if (parse_option(option, &job) != 0) {
        return 1;
    }
    job.out = out;
    job.in = in;
    job.pal = pal;
    job.gap = job.use_bayer ? palette_gap(pal) : 0.0;
    job.next_band = 0;
    job.failed = 0;
    /* Round the height up to six rows, the height of a SIXEL band. */
    job.band_height = (in->height + job.bands - 1) / job.bands;
    job.band_height = (job.band_height + 5) / 6 * 6;
    pthread_mutex_init(&job.lock, NULL);
    wanted = thread_count();
    wanted = wanted > job.bands ? job.bands : wanted;
    started = 0;
    /* The calling thread works too. If a thread cannot start, the others
     * simply take its bands. */
    for (i = 1; i < wanted; i++) {
        if (pthread_create(&threads[started], NULL, worker, &job) == 0) {
            started++;
        }
    }
    worker(&job);
    for (i = 0; i < started; i++) {
        pthread_join(threads[i], NULL);
    }
    pthread_mutex_destroy(&job.lock);
    return job.failed;
}
