/* SPDX-License-Identifier: MIT */
/*
 * Keep the threads.
 *
 * The parallel chapters start a thread for every range of bands and wait
 * for it to end, once per picture. Starting and joining a thread costs tens
 * of microseconds, which is a noticeable part of a picture that takes one or
 * two milliseconds, and a video pays it for every frame. This file starts
 * the threads once, the first time they are needed, and keeps them waiting
 * for the next picture. Encoder and decoder share them; the work of each
 * thread is the parallel-bands and parallel-decoder chapters' unchanged.
 * The threads end with the program.
 *
 * Set TUTORIAL_THREADS to override the thread count.
 */
#include <pthread.h>
#include <stdint.h>
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
/* Threads that stay                                                   */

typedef void *(*job_fn)(void *);

/* The caller describes a round of jobs and bumps the generation; thread i
 * runs job i of that round and counts itself out. Job 0 is the caller's. */
static struct {
    pthread_mutex_t lock;
    pthread_cond_t start, finish;
    unsigned long generation;
    unsigned long born;     /* the generation before a new thread's first */
    int threads;            /* started so far, job 1 to threads */
    int pending;            /* threads still running this round */
    job_fn fn;
    char *jobs;
    size_t size;
    int count;
} pool = { PTHREAD_MUTEX_INITIALIZER, PTHREAD_COND_INITIALIZER,
           PTHREAD_COND_INITIALIZER, 0, 0, 0, 0, NULL, NULL, 0, 0 };

static void *pool_thread(void *arg)
{
    int index = (int)(intptr_t)arg;
    unsigned long seen;

    /* The round that started this thread has not finished, because it
     * waits for this thread, so born still names the round before it. */
    pthread_mutex_lock(&pool.lock);
    seen = pool.born;
    for (;;) {
        while (pool.generation == seen) pthread_cond_wait(&pool.start, &pool.lock);
        seen = pool.generation;
        if (index >= pool.count) continue;          /* not needed this round */
        job_fn fn = pool.fn;
        void *job = pool.jobs + (size_t)index * pool.size;
        pthread_mutex_unlock(&pool.lock);
        fn(job);
        pthread_mutex_lock(&pool.lock);
        if (--pool.pending == 0) pthread_cond_signal(&pool.finish);
    }
    return NULL;
}

/* Run fn on each of the count jobs, size bytes apart, and wait for all of
 * them. Jobs for which no thread could be started run on the caller. */
static void run_jobs(job_fn fn, void *jobs, size_t size, int count)
{
    int helpers;

    pthread_mutex_lock(&pool.lock);
    pool.born = pool.generation;
    while (pool.threads < count - 1) {
        pthread_t thread;
        if (pthread_create(&thread, NULL, pool_thread,
                           (void *)(intptr_t)(pool.threads + 1)) != 0)
            break;
        pthread_detach(thread);
        pool.threads++;
    }
    helpers = count - 1 < pool.threads ? count - 1 : pool.threads;
    pool.fn = fn;
    pool.jobs = jobs;
    pool.size = size;
    pool.count = helpers + 1;
    pool.pending = helpers;
    pool.generation++;
    pthread_cond_broadcast(&pool.start);
    pthread_mutex_unlock(&pool.lock);

    fn(jobs);
    for (int i = helpers + 1; i < count; i++)
        fn((char *)jobs + (size_t)i * size);
    pthread_mutex_lock(&pool.lock);
    while (pool.pending > 0) pthread_cond_wait(&pool.finish, &pool.lock);
    pthread_mutex_unlock(&pool.lock);
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
    int bands = (img->height + 5) / 6, status = 0;
    int n = thread_count((size_t)img->width * img->height, 1 << 18, bands);

    for (int i = 0; i < n; i++) {
        jobs[i].img = img;
        jobs[i].first_band = (int)((long long)bands * i / n);
        jobs[i].end_band = (int)((long long)bands * (i + 1) / n);
    }
    run_jobs(encode_worker, jobs, sizeof(jobs[0]), n);

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

typedef struct band_start {
    size_t offset;          /* first byte of the band in the body */
    int color;              /* register selected at that point */
} band_start_t;

typedef struct prescan {
    band_start_t *starts;   /* one per '-' plus the first band */
    int nstarts, capacity;
    int width;              /* largest raster attributes */
    int height;
} prescan_t;

static int add_start(prescan_t *ps, size_t offset, int color)
{
    if (ps->nstarts == ps->capacity) {
        ps->capacity = ps->capacity ? ps->capacity * 2 : 256;
        band_start_t *grown = realloc(ps->starts, sizeof(band_start_t) * ps->capacity);
        if (grown == NULL) return -1;
        ps->starts = grown;
    }
    ps->starts[ps->nstarts++] = (band_start_t){ offset, color };
    return 0;
}

static int prescan(unsigned char const *body, unsigned char const *end, prescan_t *ps)
{
    unsigned char const *p = body, *band = body;
    int a[4], color = 0;

    if (add_start(ps, 0, 0) != 0) return -1;
    while (p < end) {
#ifdef HAVE_SIMD
        /* Everything except '-' and '"' can wait for the threads. */
        while (end - p >= 16) {
            v128 v = v_load(p);
            unsigned int hits = v_movemask(v_or(v_eq(v, v_set1('-')),
                                                v_eq(v, v_set1('"'))));
            if (hits != 0) {
                p += ctz64(hits);
                break;
            }
            p += 16;
        }
        if (p == end) break;
#endif
        if (*p == '-') {
            /* The band's last '#' selects the next band's first color. A
             * band without one leaves the color unchanged. */
            for (unsigned char const *q = p; q > band;) {
                if (*--q == '#') {
                    q++;
                    parse_params(&q, end, a, 1);
                    color = a[0] > 255 ? 255 : a[0];
                    break;
                }
            }
            band = ++p;
            if (add_start(ps, (size_t)(p - body), color) != 0) return -1;
        } else if (*p++ == '"') {
            parse_params(&p, end, a, 4);
            if (a[2] > 0 && a[3] > 0) {
                ps->width = a[2] > ps->width ? a[2] : ps->width;
                ps->height = a[3] > ps->height ? a[3] : ps->height;
            }
        }
    }
    return 0;
}

/* A color definition seen by a thread, to be applied after all have ended. */
typedef struct color_def {
    int color;
    int kind;               /* 1 is HLS, 2 is RGB */
    int v[3];
} color_def_t;

typedef struct decode_job {
    decoder_t *d;
    image_t *img;
    unsigned char const *begin;
    unsigned char const *end;
    int first_band, end_band, color;
    int painted_height;     /* one past the lowest painted row */
    int outside;            /* painted outside the canvas, or out of memory */
    int ncolors;            /* registers selected in this range */
    color_def_t *defs;      /* color definitions in stream order */
    int ndefs, defs_capacity;
} decode_job_t;

static int add_def(decode_job_t *job, int color, int const *a)
{
    if (job->ndefs == job->defs_capacity) {
        job->defs_capacity = job->defs_capacity ? job->defs_capacity * 2 : 256;
        color_def_t *grown = realloc(job->defs, sizeof(color_def_t) * job->defs_capacity);
        if (grown == NULL) return -1;
        job->defs = grown;
    }
    color_def_t *def = &job->defs[job->ndefs++];
    def->color = color;
    def->kind = a[1];
    memcpy(def->v, a + 2, sizeof(def->v));
    return 0;
}

/* The literal-lanes paint loop for bands [first_band, end_band), followed by
 * the transpose of those bands. */
static void *decode_worker(void *arg)
{
    decode_job_t *job = arg;
    decoder_t *d = job->d;
    unsigned char const *p = job->begin, *end = job->end;
    uint64_t color6 = COLOR6(job->color), *col, mask, paint;
    int a[5], plain, n, color, x = 0, i, bits, repeat, ch;
    int band = job->first_band;
    int band_rows = 0;      /* rows painted in this band */

    while (p < end) {
        if (band < d->bands) {
            col = d->columns + (size_t)band * d->width;
#ifdef HAVE_SIMD
            while (end - p >= 16 && d->width - x >= 16) {
                int lane = -1, n = paint_block(p, col + x, color6, &band_rows, &lane);
                p += n;
                x += n;
                if (n < 16) break;
            }
#endif
            while (p < end && *p >= '?' && *p <= '~' && x < d->width) {
                bits = *p++ - '?';
                mask = row_mask((unsigned int)bits);
                col[x] = (col[x] & ~mask) | (color6 & mask);
                band_rows |= bits;
                x++;
            }
            if (p == end) break;
        }
        ch = *p++;
        if (ch == '#' && parse_plain_number(&p, end, &plain)) {
            color = plain > 255 ? 255 : plain;
            color6 = COLOR6(color);
            if (color >= job->ncolors) job->ncolors = color + 1;
            continue;
        }
        repeat = 1;
        if (ch == '!') {
            if (!parse_plain_number(&p, end, &repeat)) parse_params(&p, end, &repeat, 1);
            repeat = repeat > 0 ? repeat : 1;
            if (p == end) break;
            ch = *p++;
        }
        if (ch >= '?' && ch <= '~') {
            bits = ch - '?';
            if (bits != 0) {
                if (band >= d->bands || repeat > d->width - x) {
                    job->outside = 1;
                    return NULL;
                }
                mask = row_mask((unsigned int)bits);
                paint = color6 & mask;
                col = d->columns + (size_t)band * d->width + x;
                for (i = 0; i < repeat; i++)
                    col[i] = (col[i] & ~mask) | paint;
                band_rows |= bits;
            }
            x = x + repeat > TUTORIAL_MAX_SIDE ? TUTORIAL_MAX_SIDE : x + repeat;
        } else if (ch == '$') {
            x = 0;
        } else if (ch == '-') {
            if (band_rows != 0) job->painted_height = band * 6 + bottom_row(band_rows) + 1;
            band_rows = 0;
            x = 0;
            band++;
        } else if (ch == '#') {
            n = parse_params(&p, end, a, 5);
            color = a[0] > 255 ? 255 : a[0];
            color6 = COLOR6(color);
            if (color >= job->ncolors) job->ncolors = color + 1;
            if (n == 5 && (a[1] == 1 || a[1] == 2) && add_def(job, color, a) != 0) {
                job->outside = 1;
                return NULL;
            }
        } else if (ch == '"') {
            parse_params(&p, end, a, 4);
        }
    }
    if (band_rows != 0) job->painted_height = band * 6 + bottom_row(band_rows) + 1;
    if (job->end_band > d->bands) job->end_band = d->bands;
    if (job->first_band < job->end_band)
        materialize(d, job->img, job->first_band, job->end_band);
    return NULL;
}

int decode_sixel(unsigned char const *data, size_t size, image_t *img)
{
    unsigned char const *end = data + size;
    prescan_t ps = { NULL, 0, 0, 0, 0 };
    decoder_t d = { NULL, 0, 0, 0, 0 };
    decode_job_t jobs[MAX_THREADS];
    int njobs = 0, height, rc = -1;

    img->ncolors = 0;
    unsigned char const *body = find_sixel_body(data, end);
    unsigned char const *stop = memchr(body, 0x1b, (size_t)(end - body));
    stop = stop != NULL ? stop : end;
    if (prescan(body, stop, &ps) != 0) goto end;
    if (ps.width == 0 || ps.height == 0) {
        rc = decode_serial(data, size, img);    /* no size to trust */
        goto end;
    }
    if (ps.width > TUTORIAL_MAX_SIDE || ps.height > TUTORIAL_MAX_SIDE) goto end;
    /* Allocate whole bands; the height is trimmed once paint is known. */
    d.width = ps.width;
    d.bands = (ps.height + 5) / 6;
    d.columns = calloc((size_t)d.width * d.bands, sizeof(uint64_t));
    height = d.bands * 6 > TUTORIAL_MAX_SIDE ? TUTORIAL_MAX_SIDE : d.bands * 6;
    if (d.columns == NULL || image_alloc(img, d.width, height) != 0) goto end;

    /* Cut the body into n byte ranges of similar size, each starting at a
     * band start, so every thread gets a similar amount of parsing. */
    int n = thread_count((size_t)(stop - body), 1 << 16, ps.nstarts);
    for (int i = 0, b = 0; i < n; i++) {
        size_t target = (size_t)(stop - body) * i / n;
        while (b < ps.nstarts && ps.starts[b].offset < target) b++;
        if (b == ps.nstarts || (njobs > 0 && jobs[njobs - 1].first_band == b)) continue;
        jobs[njobs] = (decode_job_t){ .d = &d, .img = img, .begin = body + ps.starts[b].offset,
                                      .first_band = b, .color = ps.starts[b].color };
        if (njobs > 0) {
            jobs[njobs - 1].end = jobs[njobs].begin - 1;     /* drop '-' */
            jobs[njobs - 1].end_band = b;
        }
        njobs++;
    }
    jobs[njobs - 1].end = stop;
    jobs[njobs - 1].end_band = ps.nstarts;

    run_jobs(decode_worker, jobs, sizeof(jobs[0]), njobs);
    height = ps.height;
    for (int i = 0; i < njobs; i++) {
        if (jobs[i].outside) height = -1;
        else if (height >= 0 && jobs[i].painted_height > height) height = jobs[i].painted_height;
    }
    if (height < 0 || height > TUTORIAL_MAX_SIDE) {
        rc = decode_serial(data, size, img);    /* paint outside the raster */
        goto end;
    }
    img->height = height;                       /* rows stay in place */
    for (int i = 0; i < njobs; i++) {
        if (jobs[i].ncolors > img->ncolors) img->ncolors = jobs[i].ncolors;
        for (int k = 0; k < jobs[i].ndefs; k++) {
            color_def_t const *def = &jobs[i].defs[k];
            if (def->kind == 1) set_hls(img, def->color, def->v[0], def->v[1], def->v[2]);
            else set_rgb_pct(img, def->color, def->v[0], def->v[1], def->v[2]);
        }
    }
    rc = 0;
end:
    for (int i = 0; i < njobs; i++) free(jobs[i].defs);
    free(ps.starts);
    free(d.columns);
    return rc;
}
