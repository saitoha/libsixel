/* SPDX-License-Identifier: MIT */
/*
 * Encode bands in parallel.
 *
 * Every band starts with an explicit color selection, so a band's bytes do
 * not depend on the band before it. Each thread encodes a contiguous range of
 * bands into its own buffer with its own work area, and the buffers are
 * joined in order. The stream is the packing chapter's for any number of
 * threads.
 *
 * The decoder is the literal-lanes chapter's serial one. Set
 * TUTORIAL_THREADS to override the thread count.
 */
#include <pthread.h>
#include <stdlib.h>
#include <unistd.h>

#define TUTORIAL_THREADED
#include "wide-stores.c"

#define MAX_THREADS 64

/* Use up to one thread per CPU, but give each at least `grain` units of
 * work so that thread start-up stays small compared with the work. */
static int thread_count(size_t work, size_t grain, int max_parts)
{
    char const *env = getenv("TUTORIAL_THREADS");
    long n = env != NULL ? atol(env) : sysconf(_SC_NPROCESSORS_ONLN);

    if (env == NULL && (size_t)n > work / grain) n = (long)(work / grain);
    if (n > max_parts) n = max_parts;
    if (n > MAX_THREADS) n = MAX_THREADS;
    return n < 1 ? 1 : (int)n;
}

/* ------------------------------------------------------------------ */
/* Encoder                                                             */

typedef struct encode_job {
    image_t const *img;
    int first_band, end_band;
    buffer_t out;
    int status;
} encode_job_t;

static void *encode_worker(void *arg)
{
    encode_job_t *job = arg;
    encoder_t e;

    job->status = encoder_init(&e, job->img);
    if (job->status != 0) return NULL;
    for (int b = job->first_band; b < job->end_band; b++)
        encode_band(&e, job->img, b * 6, &job->out);
    encoder_free(&e);
    return NULL;
}

int encode_sixel(image_t const *img, buffer_t *out)
{
    encode_job_t jobs[MAX_THREADS] = { { 0 } };
    pthread_t threads[MAX_THREADS];
    int created[MAX_THREADS];
    int bands = (img->height + 5) / 6, status = 0;
    int n = thread_count((size_t)img->width * img->height, 1 << 18, bands);

    for (int i = 0; i < n; i++) {
        jobs[i].img = img;
        jobs[i].first_band = (int)((long long)bands * i / n);
        jobs[i].end_band = (int)((long long)bands * (i + 1) / n);
    }
    /* The calling thread takes the first range. */
    for (int i = 1; i < n; i++)
        if (!(created[i] = pthread_create(&threads[i], NULL, encode_worker, &jobs[i]) == 0))
            encode_worker(&jobs[i]);
    encode_worker(&jobs[0]);
    for (int i = 1; i < n; i++)
        if (created[i]) pthread_join(threads[i], NULL);

    put_header_and_palette(img, out);
    for (int i = 0; i < n; i++) {
        status |= jobs[i].status;
        buf_reserve(out, jobs[i].out.size);
        memcpy(out->data + out->size, jobs[i].out.data, jobs[i].out.size);
        out->size += jobs[i].out.size;
        buf_free(&jobs[i].out);
    }
    buf_printf(out, "\033\\");
    return status != 0 ? -1 : 0;
}

/* ------------------------------------------------------------------ */
/* Decoder                                                             */

int decode_sixel(unsigned char const *p, size_t size, image_t *img)
{
    return decode_serial(p, size, img);
}
