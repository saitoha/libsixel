/* SPDX-License-Identifier: MIT */
/*
 * Step 6: threads.
 *
 * Bands are the natural unit of parallel work in both directions.
 *
 * Encoder: steps 2 to 5 already start every band with an explicit color
 * selection, so a band's bytes do not depend on the band before it. Each
 * thread encodes a contiguous range of bands into its own buffer with its
 * own step 5 work area, and the buffers are joined in order. The result is
 * byte-identical to steps 2 to 5 for any number of threads.
 *
 * Decoder: a band's bytes do depend on earlier ones, through the selected
 * color register, and paint may in principle go beyond the declared raster
 * size. A serial prescan looks only for '-' and '"', skipping sixteen bytes
 * at a time with the step 5 vector compares. At each '-' it searches backwards
 * for the last '#', which names the color the next band starts with. Trusting
 * the raster attributes for the canvas size, threads then paint disjoint band
 * ranges with the step 5 loop and transpose their own bands into the image.
 * They also collect the color definitions they pass, which are applied in
 * stream order once all threads have finished. A stream without raster
 * attributes, or one that paints outside them, is decoded again by the
 * serial step 5 decoder, so the result is always the same.
 *
 * Set TUTORIAL_THREADS to override the thread count.
 */
#include <pthread.h>
#include <stdlib.h>
#include <unistd.h>

#define TUTORIAL_STEP6
#include "step5.c"

#define MAX_THREADS 64

/* Use up to one thread per CPU, but give each at least `grain` units of
 * work so that thread start-up stays small compared with the work. */
static int
thread_count(size_t work, size_t grain, int max_parts)
{
    char const *env = getenv("TUTORIAL_THREADS");
    long n = env != NULL ? atol(env) : sysconf(_SC_NPROCESSORS_ONLN);

    if (env == NULL && (size_t)n > work / grain) {
        n = (long)(work / grain);
    }
    if (n > max_parts) {
        n = max_parts;
    }
    if (n > MAX_THREADS) {
        n = MAX_THREADS;
    }
    return n < 1 ? 1 : (int)n;
}

/* ------------------------------------------------------------------ */
/* Encoder                                                             */

typedef struct encode_job {
    image_t const *img;
    int first_band;
    int end_band;
    buffer_t out;
    int status;
} encode_job_t;

static void *
encode_worker(void *arg)
{
    encode_job_t *job = arg;
    encoder_t e;
    int b;

    job->status = encoder_init(&e, job->img);
    if (job->status != 0) {
        return NULL;
    }
    for (b = job->first_band; b < job->end_band; b++) {
        encode_band(&e, job->img, b * 6, &job->out);
    }
    encoder_free(&e);
    return NULL;
}

int
encode_sixel(image_t const *img, buffer_t *out)
{
    encode_job_t jobs[MAX_THREADS] = { { 0 } };
    pthread_t threads[MAX_THREADS];
    int created[MAX_THREADS];
    int bands = (img->height + 5) / 6;
    int n = thread_count((size_t)img->width * img->height, 1 << 18, bands);
    int status = 0;
    int i;

    for (i = 0; i < n; i++) {
        jobs[i].img = img;
        jobs[i].first_band = (int)((long long)bands * i / n);
        jobs[i].end_band = (int)((long long)bands * (i + 1) / n);
    }
    /* The calling thread takes the first range. */
    for (i = 1; i < n; i++) {
        created[i] = pthread_create(&threads[i], NULL, encode_worker,
                                    &jobs[i]) == 0;
        if (!created[i]) {
            encode_worker(&jobs[i]);
        }
    }
    encode_worker(&jobs[0]);
    for (i = 1; i < n; i++) {
        if (created[i]) {
            pthread_join(threads[i], NULL);
        }
    }

    put_header_and_palette(img, out);
    for (i = 0; i < n; i++) {
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
    int nstarts;
    int capacity;
    int width;              /* largest raster attributes */
    int height;
} prescan_t;

static int
add_start(prescan_t *ps, size_t offset, int color)
{
    band_start_t *grown;

    if (ps->nstarts == ps->capacity) {
        ps->capacity = ps->capacity ? ps->capacity * 2 : 256;
        grown = realloc(ps->starts, sizeof(band_start_t) * ps->capacity);
        if (grown == NULL) {
            return -1;
        }
        ps->starts = grown;
    }
    ps->starts[ps->nstarts].offset = offset;
    ps->starts[ps->nstarts].color = color;
    ps->nstarts++;
    return 0;
}

static int
prescan(unsigned char const *body, unsigned char const *end, prescan_t *ps)
{
    unsigned char const *p = body;
    unsigned char const *band = body;
    unsigned char const *q;
    int a[4];
    int color = 0;

    if (add_start(ps, 0, 0) != 0) {
        return -1;
    }
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
        if (p == end) {
            break;
        }
#endif
        if (*p == '-') {
            /* The band's last '#' selects the next band's first color. A
             * band without one leaves the color unchanged. */
            for (q = p; q > band;) {
                if (*--q == '#') {
                    q++;
                    parse_params(&q, end, a, 1);
                    color = a[0] > 255 ? 255 : a[0];
                    break;
                }
            }
            band = ++p;
            if (add_start(ps, (size_t)(p - body), color) != 0) {
                return -1;
            }
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
    int first_band;
    int end_band;
    int color;
    int painted_height;     /* one past the lowest painted row */
    int outside;            /* painted outside the canvas, or out of memory */
    int ncolors;            /* registers selected in this range */
    color_def_t *defs;      /* color definitions in stream order */
    int ndefs;
    int defs_capacity;
} decode_job_t;

static int
add_def(decode_job_t *job, int color, int const *a)
{
    color_def_t *grown;

    if (job->ndefs == job->defs_capacity) {
        job->defs_capacity = job->defs_capacity ? job->defs_capacity * 2 : 256;
        grown = realloc(job->defs, sizeof(color_def_t) * job->defs_capacity);
        if (grown == NULL) {
            return -1;
        }
        job->defs = grown;
    }
    job->defs[job->ndefs].color = color;
    job->defs[job->ndefs].kind = a[1];
    memcpy(job->defs[job->ndefs].v, a + 2, sizeof(job->defs[0].v));
    job->ndefs++;
    return 0;
}

/* The step 5 paint loop for bands [first_band, end_band), followed by the
 * transpose of those bands. */
static void *
decode_worker(void *arg)
{
    decode_job_t *job = arg;
    decoder_t *d = job->d;
    unsigned char const *p = job->begin;
    unsigned char const *end = job->end;
    uint64_t color6 = (uint64_t)job->color * UINT64_C(0x010101010101);
    uint64_t *col;
    uint64_t mask;
    uint64_t paint;
    int a[5];
    int band = job->first_band;
    int band_rows = 0;      /* rows painted in this band */
    int plain;
    int n;
    int color;
    int x = 0;
    int i;
    int bits;
    int repeat;
    int ch;

    while (p < end) {
        if (band < d->bands) {
            col = d->columns + (size_t)band * d->width;
            while (p < end && *p >= '?' && *p <= '~' && x < d->width) {
                bits = *p++ - '?';
                mask = row_mask((unsigned int)bits);
                col[x] = (col[x] & ~mask) | (color6 & mask);
                band_rows |= bits;
                x++;
            }
            if (p == end) {
                break;
            }
        }
        ch = *p++;
        if (ch == '#' && parse_plain_number(&p, end, &plain)) {
            color = plain > 255 ? 255 : plain;
            color6 = (uint64_t)color * UINT64_C(0x010101010101);
            if (color >= job->ncolors) {
                job->ncolors = color + 1;
            }
            continue;
        }
        repeat = 1;
        if (ch == '!') {
            if (!parse_plain_number(&p, end, &repeat)) {
                parse_params(&p, end, &repeat, 1);
            }
            repeat = repeat > 0 ? repeat : 1;
            if (p == end) {
                break;
            }
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
                for (i = 0; i < repeat; i++) {
                    col[i] = (col[i] & ~mask) | paint;
                }
                band_rows |= bits;
            }
            x = x + repeat > TUTORIAL_MAX_SIDE ? TUTORIAL_MAX_SIDE : x + repeat;
        } else if (ch == '$') {
            x = 0;
        } else if (ch == '-') {
            if (band_rows != 0) {
                job->painted_height = band * 6 + bottom_row(band_rows) + 1;
            }
            band_rows = 0;
            x = 0;
            band++;
        } else if (ch == '#') {
            n = parse_params(&p, end, a, 5);
            color = a[0] > 255 ? 255 : a[0];
            color6 = (uint64_t)color * UINT64_C(0x010101010101);
            if (color >= job->ncolors) {
                job->ncolors = color + 1;
            }
            if (n == 5 && (a[1] == 1 || a[1] == 2) &&
                    add_def(job, color, a) != 0) {
                job->outside = 1;
                return NULL;
            }
        } else if (ch == '"') {
            parse_params(&p, end, a, 4);
        }
    }
    if (band_rows != 0) {
        job->painted_height = band * 6 + bottom_row(band_rows) + 1;
    }
    if (job->end_band > d->bands) {
        job->end_band = d->bands;
    }
    if (job->first_band < job->end_band) {
        materialize(d, job->img, job->first_band, job->end_band);
    }
    return NULL;
}

int
decode_sixel(unsigned char const *data, size_t size, image_t *img)
{
    unsigned char const *p = data;
    unsigned char const *end = data + size;
    unsigned char const *body;
    unsigned char const *stop;
    prescan_t ps = { NULL, 0, 0, 0, 0 };
    decoder_t d = { NULL, 0, 0, 0, 0 };
    decode_job_t jobs[MAX_THREADS];
    pthread_t threads[MAX_THREADS];
    int created[MAX_THREADS];
    int njobs = 0;
    int height;
    int n;
    int b;
    int i;
    int rc = -1;

    img->ncolors = 0;
    body = find_sixel_body(p, end);
    stop = memchr(body, 0x1b, (size_t)(end - body));
    stop = stop != NULL ? stop : end;
    if (prescan(body, stop, &ps) != 0) {
        goto end;
    }
    if (ps.width == 0 || ps.height == 0) {
        rc = decode_serial(data, size, img);    /* no size to trust */
        goto end;
    }
    if (ps.width > TUTORIAL_MAX_SIDE || ps.height > TUTORIAL_MAX_SIDE) {
        goto end;
    }
    /* Allocate whole bands; the height is trimmed once paint is known. */
    d.width = ps.width;
    d.bands = (ps.height + 5) / 6;
    d.columns = calloc((size_t)d.width * d.bands, sizeof(uint64_t));
    height = d.bands * 6 > TUTORIAL_MAX_SIDE ? TUTORIAL_MAX_SIDE : d.bands * 6;
    if (d.columns == NULL || image_alloc(img, d.width, height) != 0) {
        goto end;
    }

    /* Cut the body into n byte ranges of similar size, each starting at a
     * band start, so every thread gets a similar amount of parsing. */
    n = thread_count((size_t)(stop - body), 1 << 16, ps.nstarts);
    for (i = 0, b = 0; i < n; i++) {
        size_t target = (size_t)(stop - body) * i / n;

        while (b < ps.nstarts && ps.starts[b].offset < target) {
            b++;
        }
        if (b == ps.nstarts || (njobs > 0 && jobs[njobs - 1].first_band == b)) {
            continue;
        }
        memset(&jobs[njobs], 0, sizeof(jobs[njobs]));
        jobs[njobs].d = &d;
        jobs[njobs].img = img;
        jobs[njobs].begin = body + ps.starts[b].offset;
        jobs[njobs].first_band = b;
        jobs[njobs].color = ps.starts[b].color;
        if (njobs > 0) {
            jobs[njobs - 1].end = jobs[njobs].begin - 1;     /* drop '-' */
            jobs[njobs - 1].end_band = b;
        }
        njobs++;
    }
    jobs[njobs - 1].end = stop;
    jobs[njobs - 1].end_band = ps.nstarts;

    for (i = 1; i < njobs; i++) {
        created[i] = pthread_create(&threads[i], NULL, decode_worker,
                                    &jobs[i]) == 0;
        if (!created[i]) {
            decode_worker(&jobs[i]);
        }
    }
    decode_worker(&jobs[0]);
    height = ps.height;
    for (i = 0; i < njobs; i++) {
        if (i > 0 && created[i]) {
            pthread_join(threads[i], NULL);
        }
        if (jobs[i].outside) {
            height = -1;
        } else if (height >= 0 && jobs[i].painted_height > height) {
            height = jobs[i].painted_height;
        }
    }
    if (height < 0 || height > TUTORIAL_MAX_SIDE) {
        rc = decode_serial(data, size, img);    /* paint outside the raster */
        goto end;
    }
    img->height = height;                       /* rows stay in place */
    for (i = 0; i < njobs; i++) {
        if (jobs[i].ncolors > img->ncolors) {
            img->ncolors = jobs[i].ncolors;
        }
        for (b = 0; b < jobs[i].ndefs; b++) {
            color_def_t const *def = &jobs[i].defs[b];

            if (def->kind == 1) {
                set_hls(img, def->color, def->v[0], def->v[1], def->v[2]);
            } else {
                set_rgb_pct(img, def->color, def->v[0], def->v[1], def->v[2]);
            }
        }
    }
    rc = 0;
end:
    for (i = 0; i < njobs; i++) {
        free(jobs[i].defs);
    }
    free(ps.starts);
    free(d.columns);
    return rc;
}
