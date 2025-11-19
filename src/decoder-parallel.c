#include "config.h"

#include <errno.h>
#include <limits.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <sixel.h>

#include "allocator.h"
#include "decoder-image.h"
#include "decoder-parallel.h"
#include "decoder-prescan.h"
#include "output.h"
#if SIXEL_ENABLE_THREADS
# include "sixel_threading.h"
# include "threadpool.h"
#endif

#define SIXEL_PARALLEL_MIN_BYTES   2048
#define SIXEL_PARALLEL_MIN_PIXELS  (64 * 64)
#define SIXEL_PARALLEL_MIN_BANDS   2
#define SIXEL_PARALLEL_MIN_BAND_BYTES 512
#define SIXEL_PARALLEL_MIN_PIXELS_PER_THREAD (128 * 128)
#define SIXEL_PARALLEL_MIN_JOBS_PER_THREAD 4
#define SIXEL_PARALLEL_MAX_REPEAT  0xffff
#define SIXEL_PARALLEL_PALVAL(n, a, m) \
    (((n) * (a) + ((m) / 2)) / (m))
#define SIXEL_PARALLEL_RGBA(r, g, b, a) \
    (((uint32_t)(r) << 24) | ((uint32_t)(g) << 16) | \
     ((uint32_t)(b) << 8) | ((uint32_t)(a)))
#define SIXEL_PARALLEL_XRGB(r, g, b) \
    SIXEL_PARALLEL_RGBA(SIXEL_PARALLEL_PALVAL((r), 255, 100), \
                        SIXEL_PARALLEL_PALVAL((g), 255, 100), \
                        SIXEL_PARALLEL_PALVAL((b), 255, 100), \
                        255)

typedef struct sixel_decoder_thread_config {
    int env_checked;
    int env_valid;
    int env_threads;
    int override_active;
    int override_threads;
} sixel_decoder_thread_config_t;

static sixel_decoder_thread_config_t g_decoder_threads = {
    0,
    0,
    1,
    0,
    1
};

static int
sixel_decoder_threads_token_is_auto(char const *text)
{
    if (text == NULL) {
        return 0;
    }
    if ((text[0] == 'a' || text[0] == 'A') &&
            (text[1] == 'u' || text[1] == 'U') &&
            (text[2] == 't' || text[2] == 'T') &&
            (text[3] == 'o' || text[3] == 'O') &&
            text[4] == '\0') {
        return 1;
    }
    return 0;
}

static int
sixel_decoder_threads_normalize(int requested)
{
    int normalized;

#if SIXEL_ENABLE_THREADS
    int hw_threads;

    if (requested <= 0) {
        hw_threads = sixel_get_hw_threads();
        if (hw_threads < 1) {
            hw_threads = 1;
        }
        normalized = hw_threads;
    } else {
        normalized = requested;
    }
#else
    (void)requested;
    normalized = 1;
#endif
    if (normalized < 1) {
        normalized = 1;
    }
    return normalized;
}

static int
sixel_decoder_threads_parse_value(char const *text, int *value)
{
    long parsed;
    char *endptr;
    int normalized;

    if (text == NULL || value == NULL) {
        return 0;
    }
    if (sixel_decoder_threads_token_is_auto(text)) {
        normalized = sixel_decoder_threads_normalize(0);
        *value = normalized;
        return 1;
    }
    errno = 0;
    parsed = strtol(text, &endptr, 10);
    if (endptr == text || *endptr != '\0' || errno == ERANGE) {
        return 0;
    }
    if (parsed < 1) {
        normalized = sixel_decoder_threads_normalize(1);
    } else if (parsed > INT_MAX) {
        normalized = sixel_decoder_threads_normalize(INT_MAX);
    } else {
        normalized = sixel_decoder_threads_normalize((int)parsed);
    }
    *value = normalized;
    return 1;
}

static void
sixel_decoder_threads_load_env(void)
{
    char const *text;
    int parsed;

    if (g_decoder_threads.env_checked) {
        return;
    }
    g_decoder_threads.env_checked = 1;
    g_decoder_threads.env_valid = 0;
    text = getenv("SIXEL_THREADS");
    if (text == NULL || text[0] == '\0') {
        return;
    }
    if (sixel_decoder_threads_parse_value(text, &parsed)) {
        g_decoder_threads.env_threads = parsed;
        g_decoder_threads.env_valid = 1;
    }
}

SIXELSTATUS
sixel_decoder_parallel_override_threads(char const *text)
{
    SIXELSTATUS status;
    int parsed;

    status = SIXEL_BAD_ARGUMENT;
    if (text == NULL || text[0] == '\0') {
        sixel_helper_set_additional_message(
            "decoder: missing thread count after -=/--threads.");
        goto end;
    }
    if (!sixel_decoder_threads_parse_value(text, &parsed)) {
        sixel_helper_set_additional_message(
            "decoder: threads must be a positive integer or 'auto'.");
        goto end;
    }
    g_decoder_threads.override_active = 1;
    g_decoder_threads.override_threads = parsed;
    status = SIXEL_OK;
end:
    return status;
}

#if SIXEL_ENABLE_THREADS

typedef struct sixel_parallel_decode_plan {
    unsigned char *data;
    int len;
    image_buffer_t *image;
    sixel_allocator_t *allocator;
    sixel_prescan_t *prescan;
    int depth;
    int threads;
    int direct_mode;
    int width;
    int height;
    int *band_color_max;
    int band_count;
} sixel_parallel_decode_plan_t;

static SIXELSTATUS sixel_parallel_safe_addition(
    sixel_prescan_band_state_t *state,
    unsigned char value)
{
    SIXELSTATUS status;
    int digit;

    status = SIXEL_FALSE;
    digit = (int)value - '0';
    if ((state->param > INT_MAX / 10) ||
            (digit > INT_MAX - state->param * 10)) {
        status = SIXEL_BAD_INTEGER_OVERFLOW;
        sixel_helper_set_additional_message(
            "decoder-parallel: integer overflow in parameter.");
        goto end;
    }
    state->param = state->param * 10 + digit;
    status = SIXEL_OK;
end:
    return status;
}

static uint32_t sixel_parallel_hls_to_rgba(int hue, int lum, int sat)
{
    double min;
    double max;
    int r;
    int g;
    int b;

    if (sat == 0) {
        r = g = b = lum;
    }
    max = lum + sat *
        (1.0 - (lum > 50 ? (2 * (lum / 100.0) - 1.0) :
                - (2 * (lum / 100.0) - 1.0))) / 2.0;
    min = lum - sat *
        (1.0 - (lum > 50 ? (2 * (lum / 100.0) - 1.0) :
                - (2 * (lum / 100.0) - 1.0))) / 2.0;
    hue = (hue + 240) % 360;
    switch (hue / 60) {
    case 0:
        r = max;
        g = (min + (max - min) * (hue / 60.0));
        b = min;
        break;
    case 1:
        r = min + (max - min) * ((120 - hue) / 60.0);
        g = max;
        b = min;
        break;
    case 2:
        r = min;
        g = max;
        b = (min + (max - min) * ((hue - 120) / 60.0));
        break;
    case 3:
        r = min;
        g = (min + (max - min) * ((240 - hue) / 60.0));
        b = max;
        break;
    case 4:
        r = (min + (max - min) * ((hue - 240) / 60.0));
        g = min;
        b = max;
        break;
    case 5:
        r = max;
        g = min;
        b = (min + (max - min) * ((360 - hue) / 60.0));
        break;
    default:
#if HAVE___BUILTIN_UNREACHABLE
        __builtin_unreachable();
#endif
        r = g = b = 0;
        break;
    }
    return SIXEL_PARALLEL_RGBA(r, g, b, 255);
}

static void sixel_parallel_store_indexed(image_buffer_t *image,
                                         int x,
                                         int y,
                                         int repeat,
                                         int color_index)
{
    size_t offset;

    if (repeat <= 0 || image == NULL) {
        return;
    }
    offset = (size_t)image->width * (size_t)y + (size_t)x;
    memset(image->pixels.in_bytes + offset,
           color_index,
           (size_t)repeat);
}

static void sixel_parallel_store_rgba(image_buffer_t *image,
                                      int x,
                                      int y,
                                      int repeat,
                                      uint32_t rgba)
{
    unsigned char r;
    unsigned char g;
    unsigned char b;
    unsigned char a;
    unsigned char *dst;
    size_t offset;
    int i;

    if (repeat <= 0 || image == NULL) {
        return;
    }
    r = (unsigned char)(rgba >> 24);
    g = (unsigned char)((rgba >> 16) & 0xff);
    b = (unsigned char)((rgba >> 8) & 0xff);
    a = (unsigned char)(rgba & 0xff);
    offset = ((size_t)image->width * (size_t)y + (size_t)x) * 4u;
    dst = image->pixels.in_bytes + offset;
    for (i = 0; i < repeat; ++i) {
        dst[0] = r;
        dst[1] = g;
        dst[2] = b;
        dst[3] = a;
        dst += 4;
    }
}

static void sixel_parallel_fill_span(sixel_parallel_decode_plan_t *plan,
                                     int y,
                                     int x,
                                     int repeat,
                                     int color_index,
                                     uint32_t *palette)
{
    if (y < 0 || y >= plan->height) {
        return;
    }
    if (color_index < 0) {
        color_index = 0;
    }
    if (color_index >= SIXEL_PRESCAN_PALETTE_MAX) {
        color_index = SIXEL_PRESCAN_PALETTE_MAX - 1;
    }
    if (plan->direct_mode) {
        sixel_parallel_store_rgba(plan->image,
                                  x,
                                  y,
                                  repeat,
                                  palette[color_index]);
    } else {
        sixel_parallel_store_indexed(plan->image,
                                     x,
                                     y,
                                     repeat,
                                     color_index);
    }
}

static void sixel_parallel_track_color(int color_index,
                                       int *local_max_color)
{
    if (color_index > *local_max_color) {
        *local_max_color = color_index;
    }
}

static SIXELSTATUS sixel_parallel_decode_band(
    sixel_parallel_decode_plan_t *plan,
    int band_index)
{
    SIXELSTATUS status;
    sixel_prescan_band_state_t state;
    unsigned char *cursor;
    unsigned char *end;
    int bits;
    int mask;
    int i;
    int local_max_color;

    status = SIXEL_FALSE;
    state = plan->prescan->band_states[band_index];
    if ((size_t)plan->len < plan->prescan->band_start_offsets[band_index] ||
            (size_t)plan->len < plan->prescan->band_end_offsets[band_index] ||
            plan->prescan->band_end_offsets[band_index] <
                plan->prescan->band_start_offsets[band_index]) {
        sixel_helper_set_additional_message(
            "decoder-parallel: invalid band boundaries detected.");
        status = SIXEL_BAD_INPUT;
        goto end;
    }
    cursor = plan->data + plan->prescan->band_start_offsets[band_index];
    end = plan->data + plan->prescan->band_end_offsets[band_index];
    local_max_color = -1;

    while (cursor < end) {
        switch (state.state) {
        case SIXEL_PRESCAN_PS_GROUND:
            switch (*cursor) {
            case 0x1b:
                state.state = SIXEL_PRESCAN_PS_ESC;
                break;
            case 0x90:
                state.state = SIXEL_PRESCAN_PS_DCS;
                break;
            case 0x9c:
                cursor = end;
                continue;
            default:
                break;
            }
            break;
        case SIXEL_PRESCAN_PS_ESC:
            switch (*cursor) {
            case '\\':
            case 0x9c:
                cursor = end;
                continue;
            case 'P':
                state.param = -1;
                state.state = SIXEL_PRESCAN_PS_DCS;
                break;
            default:
                break;
            }
            break;
        case SIXEL_PRESCAN_PS_DCS:
            switch (*cursor) {
            case 0x1b:
                state.state = SIXEL_PRESCAN_PS_ESC;
                break;
            case '0':
            case '1':
            case '2':
            case '3':
            case '4':
            case '5':
            case '6':
            case '7':
            case '8':
            case '9':
                if (state.param < 0) {
                    state.param = 0;
                }
                status = sixel_parallel_safe_addition(&state, *cursor);
                if (SIXEL_FAILED(status)) {
                    goto end;
                }
                break;
            case ';':
                if (state.param < 0) {
                    state.param = 0;
                }
                if (state.nparams < DECSIXEL_PARAMS_MAX) {
                    state.params[state.nparams++] = state.param;
                }
                state.param = 0;
                break;
            case 'q':
                if (state.param >= 0 &&
                        state.nparams < DECSIXEL_PARAMS_MAX) {
                    state.params[state.nparams++] = state.param;
                }
                state.param = 0;
                state.state = SIXEL_PRESCAN_PS_DECSIXEL;
                break;
            default:
                break;
            }
            break;
        case SIXEL_PRESCAN_PS_DECSIXEL:
            switch (*cursor) {
            case 0x1b:
                state.state = SIXEL_PRESCAN_PS_ESC;
                break;
            case '"':
                state.param = 0;
                state.nparams = 0;
                state.state = SIXEL_PRESCAN_PS_DECGRA;
                break;
            case '!':
                state.param = 0;
                state.nparams = 0;
                state.state = SIXEL_PRESCAN_PS_DECGRI;
                break;
            case '#':
                state.param = 0;
                state.nparams = 0;
                state.state = SIXEL_PRESCAN_PS_DECGCI;
                break;
            case '$':
                state.pos_x = 0;
                break;
            case '-':
                state.pos_x = 0;
                state.pos_y += 6;
                break;
            default:
                if (*cursor >= '?' && *cursor <= '~') {
                    bits = *cursor - '?';
                    if (state.pos_x < 0 || state.pos_y < 0) {
                        status = SIXEL_BAD_INPUT;
                        sixel_helper_set_additional_message(
                            "decoder-parallel: negative draw position.");
                        goto end;
                    }
                    if (bits == 0) {
                        state.pos_x += state.repeat_count;
                    } else {
                        mask = 0x01;
                        if (state.repeat_count <= 1) {
                            for (i = 0; i < 6; ++i) {
                                if ((bits & mask) != 0) {
                                    sixel_parallel_fill_span(plan,
                                                            state.pos_y + i,
                                                            state.pos_x,
                                                            1,
                                                            state.color_index,
                                                            state.palette);
                                    sixel_parallel_track_color(
                                        state.color_index,
                                        &local_max_color);
                                }
                                mask <<= 1;
                            }
                            state.pos_x += 1;
                        } else {
                            for (i = 0; i < 6; ++i) {
                                if ((bits & mask) != 0) {
                                    int c;
                                    int run_span;
                                    int row;

                                    c = mask << 1;
                                    run_span = 1;
                                    while ((i + run_span) < 6 &&
                                            (bits & c) != 0) {
                                        c <<= 1;
                                        run_span += 1;
                                    }
                                    for (row = 0; row < run_span; ++row) {
                                        sixel_parallel_fill_span(
                                            plan,
                                            state.pos_y + i + row,
                                            state.pos_x,
                                            state.repeat_count,
                                            state.color_index,
                                            state.palette);
                                    }
                                    sixel_parallel_track_color(
                                        state.color_index,
                                        &local_max_color);
                                    i += run_span - 1;
                                    mask <<= run_span - 1;
                                }
                                mask <<= 1;
                            }
                            state.pos_x += state.repeat_count;
                        }
                    }
                    state.repeat_count = 1;
                }
                break;
            }
            break;
        case SIXEL_PRESCAN_PS_DECGRA:
            switch (*cursor) {
            case 0x1b:
                state.state = SIXEL_PRESCAN_PS_ESC;
                break;
            case '0':
            case '1':
            case '2':
            case '3':
            case '4':
            case '5':
            case '6':
            case '7':
            case '8':
            case '9':
                status = sixel_parallel_safe_addition(&state, *cursor);
                if (SIXEL_FAILED(status)) {
                    goto end;
                }
                break;
            case ';':
                if (state.nparams < DECSIXEL_PARAMS_MAX) {
                    state.params[state.nparams++] = state.param;
                }
                state.param = 0;
                break;
            default:
                if (state.nparams < DECSIXEL_PARAMS_MAX) {
                    state.params[state.nparams++] = state.param;
                }
                if (state.nparams > 0) {
                    state.attributed_pad = state.params[0];
                }
                if (state.nparams > 1) {
                    state.attributed_pan = state.params[1];
                }
                if (state.nparams > 2 && state.params[2] > 0) {
                    state.attributed_ph = state.params[2];
                }
                if (state.nparams > 3 && state.params[3] > 0) {
                    state.attributed_pv = state.params[3];
                }
                if (state.attributed_pan <= 0) {
                    state.attributed_pan = 1;
                }
                if (state.attributed_pad <= 0) {
                    state.attributed_pad = 1;
                }
                state.param = 0;
                state.nparams = 0;
                state.state = SIXEL_PRESCAN_PS_DECSIXEL;
                continue;
            }
            break;
        case SIXEL_PRESCAN_PS_DECGRI:
            switch (*cursor) {
            case 0x1b:
                state.state = SIXEL_PRESCAN_PS_ESC;
                break;
            case '0':
            case '1':
            case '2':
            case '3':
            case '4':
            case '5':
            case '6':
            case '7':
            case '8':
            case '9':
                status = sixel_parallel_safe_addition(&state, *cursor);
                if (SIXEL_FAILED(status)) {
                    goto end;
                }
                break;
            case ';':
                break;
            default:
                state.repeat_count = state.param;
                if (state.repeat_count == 0) {
                    state.repeat_count = 1;
                }
                if (state.repeat_count > SIXEL_PARALLEL_MAX_REPEAT) {
                    status = SIXEL_BAD_INPUT;
                    sixel_helper_set_additional_message(
                        "decoder-parallel: repeat parameter too large.");
                    goto end;
                }
                state.param = 0;
                state.nparams = 0;
                state.state = SIXEL_PRESCAN_PS_DECSIXEL;
                continue;
            }
            break;
        case SIXEL_PRESCAN_PS_DECGCI:
            switch (*cursor) {
            case 0x1b:
                state.state = SIXEL_PRESCAN_PS_ESC;
                break;
            case '0':
            case '1':
            case '2':
            case '3':
            case '4':
            case '5':
            case '6':
            case '7':
            case '8':
            case '9':
                status = sixel_parallel_safe_addition(&state, *cursor);
                if (SIXEL_FAILED(status)) {
                    goto end;
                }
                break;
            case ';':
                if (state.nparams < DECSIXEL_PARAMS_MAX) {
                    state.params[state.nparams++] = state.param;
                }
                state.param = 0;
                break;
            default:
                if (state.nparams < DECSIXEL_PARAMS_MAX) {
                    state.params[state.nparams++] = state.param;
                }
                state.param = 0;
                if (state.nparams > 0) {
                    state.color_index = state.params[0];
                    if (state.color_index < 0) {
                        state.color_index = 0;
                    } else if (state.color_index >=
                            SIXEL_PRESCAN_PALETTE_MAX) {
                        state.color_index = SIXEL_PRESCAN_PALETTE_MAX - 1;
                    }
                }
                if (state.nparams > 4) {
                    if (state.params[1] == 1) {
                        if (state.params[2] > 360) {
                            state.params[2] = 360;
                        }
                        if (state.params[3] > 100) {
                            state.params[3] = 100;
                        }
                        if (state.params[4] > 100) {
                            state.params[4] = 100;
                        }
                        state.palette[state.color_index] =
                            sixel_parallel_hls_to_rgba(state.params[2],
                                                       state.params[3],
                                                       state.params[4]);
                    } else if (state.params[1] == 2) {
                        if (state.params[2] > 100) {
                            state.params[2] = 100;
                        }
                        if (state.params[3] > 100) {
                            state.params[3] = 100;
                        }
                        if (state.params[4] > 100) {
                            state.params[4] = 100;
                        }
                        state.palette[state.color_index] =
                            SIXEL_PARALLEL_XRGB(state.params[2],
                                                state.params[3],
                                                state.params[4]);
                    }
                }
                state.nparams = 0;
                state.state = SIXEL_PRESCAN_PS_DECSIXEL;
                continue;
            }
            break;
        default:
            break;
        }
        cursor++;
    }
    plan->band_color_max[band_index] = local_max_color;
    status = SIXEL_OK;
end:
    return status;
}

static int sixel_parallel_worker(tp_job_t job,
                                 void *userdata,
                                 void *workspace)
{
    sixel_parallel_decode_plan_t *plan;
    SIXELSTATUS status;

    (void)workspace;
    plan = (sixel_parallel_decode_plan_t *)userdata;
    status = sixel_parallel_decode_band(plan, job.band_index);
    if (SIXEL_FAILED(status)) {
        return -1;
    }
    return 0;
}

static SIXELSTATUS sixel_parallel_run_workers(
    sixel_parallel_decode_plan_t *plan)
{
    SIXELSTATUS status;
    threadpool_t *pool;
    int worker_threads;
    int job_index;

    status = SIXEL_FALSE;
    pool = NULL;
    worker_threads = plan->threads;
    if (worker_threads > plan->band_count) {
        worker_threads = plan->band_count;
    }
    if (worker_threads < 1) {
        worker_threads = 1;
    }
    if (worker_threads == 1) {
        for (job_index = 0; job_index < plan->band_count;
                ++job_index) {
            status = sixel_parallel_decode_band(plan, job_index);
            if (SIXEL_FAILED(status)) {
                goto end;
            }
        }
        status = SIXEL_OK;
        goto end;
    }
    pool = threadpool_create(worker_threads,
                             plan->band_count,
                             0,
                             sixel_parallel_worker,
                             plan);
    if (pool == NULL) {
        sixel_helper_set_additional_message(
            "decoder-parallel: failed to create threadpool.");
        status = SIXEL_BAD_ALLOCATION;
        goto end;
    }
    for (job_index = 0; job_index < plan->band_count;
            ++job_index) {
        threadpool_push(pool, (tp_job_t){ job_index });
    }
    threadpool_finish(pool);
    if (threadpool_get_error(pool) != 0) {
        sixel_helper_set_additional_message(
            "decoder-parallel: worker reported an error.");
        status = SIXEL_BAD_INPUT;
        goto end;
    }
    status = SIXEL_OK;
end:
    if (pool != NULL) {
        threadpool_destroy(pool);
    }
    return status;
}

static int sixel_parallel_should_attempt(sixel_prescan_t *prescan,
                                         int len,
                                         int threads)
{
    int pixels;
    int pixels_per_thread;
    int avg_band_bytes;
    int jobs_per_thread;

    if (prescan == NULL) {
        return 0;
    }
    if (threads < 2) {
        return 0;
    }
    if (len < SIXEL_PARALLEL_MIN_BYTES) {
        return 0;
    }
    if (prescan->band_count < SIXEL_PARALLEL_MIN_BANDS) {
        return 0;
    }
    if (prescan->flags != 0u) {
        return 0;
    }
    pixels = prescan->width * prescan->height;
    if (pixels < SIXEL_PARALLEL_MIN_PIXELS) {
        return 0;
    }
    pixels_per_thread = pixels / threads;
    if (pixels_per_thread < SIXEL_PARALLEL_MIN_PIXELS_PER_THREAD) {
        return 0;
    }
    jobs_per_thread = prescan->band_count / threads;
    if (jobs_per_thread < SIXEL_PARALLEL_MIN_JOBS_PER_THREAD) {
        return 0;
    }
    avg_band_bytes = len / prescan->band_count;
    if (avg_band_bytes < SIXEL_PARALLEL_MIN_BAND_BYTES) {
        return 0;
    }
    return 1;
}

static SIXELSTATUS sixel_parallel_finalize_palette(image_buffer_t *image,
                                                   int *band_color_max,
                                                   int band_count)
{
    int max_color;
    int i;

    if (image == NULL || band_color_max == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    max_color = 0;
    for (i = 0; i < band_count; ++i) {
        if (band_color_max[i] > max_color) {
            max_color = band_color_max[i];
        }
    }
    if (max_color < 0) {
        max_color = 0;
    }
    image->ncolors = max_color;
    return SIXEL_OK;
}

static void sixel_parallel_apply_final_palette(image_buffer_t *image,
                                               sixel_prescan_t *prescan)
{
    uint32_t rgba;
    int entries;
    int r;
    int g;
    int b;
    int i;

    if (image == NULL || prescan == NULL) {
        return;
    }
    entries = SIXEL_PRESCAN_PALETTE_MAX;
    if (entries > SIXEL_PALETTE_MAX_DECODER) {
        entries = SIXEL_PALETTE_MAX_DECODER;
    }
    for (i = 0; i < entries; ++i) {
        rgba = prescan->final_state.palette[i];
        r = (int)((rgba >> 24) & 0xff);
        g = (int)((rgba >> 16) & 0xff);
        b = (int)((rgba >> 8) & 0xff);
        image->palette[i] = (r << 16) | (g << 8) | b;
    }
}

int
sixel_decoder_parallel_resolve_threads(void)
{
    sixel_decoder_threads_load_env();
    if (g_decoder_threads.override_active) {
        return g_decoder_threads.override_threads;
    }
    if (g_decoder_threads.env_valid) {
        return g_decoder_threads.env_threads;
    }
    return 1;
}

static SIXELSTATUS sixel_parallel_decode_internal(
    unsigned char *p,
    int len,
    image_buffer_t *image,
    sixel_allocator_t *allocator,
    int depth,
    int *used_parallel)
{
    SIXELSTATUS status;
    sixel_prescan_t *prescan;
    sixel_parallel_decode_plan_t plan;
    int bgindex;
    int width;
    int height;
    int band_count;
    int threads;

    status = SIXEL_FALSE;
    prescan = NULL;
    memset(&plan, 0, sizeof(plan));
    if (used_parallel != NULL) {
        *used_parallel = 0;
    }
    if (image == NULL || allocator == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    threads = sixel_decoder_parallel_resolve_threads();
    if (threads < 2 || len < SIXEL_PARALLEL_MIN_BYTES) {
        status = SIXEL_OK;
        goto end;
    }
    status = sixel_prescan_run(p, len, &prescan, allocator);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    if (!sixel_parallel_should_attempt(prescan,
                                       len,
                                       threads)) {
        status = SIXEL_OK;
        goto end;
    }
    width = prescan->width;
    height = prescan->height;
    if (width < 1) {
        width = 1;
    }
    if (height < 1) {
        height = 1;
    }
    bgindex = prescan->band_states[0].bgindex;
    status = image_buffer_init(image,
                               width,
                               height,
                               bgindex,
                               depth,
                               allocator);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    plan.data = p;
    plan.len = len;
    plan.image = image;
    plan.allocator = allocator;
    plan.prescan = prescan;
    plan.depth = depth;
    plan.direct_mode = (depth == 4);
    plan.width = width;
    plan.height = height;
    plan.threads = threads;
    band_count = prescan->band_count;
    if (band_count < 1) {
        status = SIXEL_OK;
        goto end;
    }
    plan.band_count = band_count;
    plan.band_color_max = (int *)sixel_allocator_calloc(allocator,
                                                        (size_t)band_count,
                                                        sizeof(int));
    if (plan.band_color_max == NULL) {
        sixel_helper_set_additional_message(
            "decoder-parallel: failed to allocate band metadata.");
        status = SIXEL_BAD_ALLOCATION;
        goto end;
    }
    status = sixel_parallel_run_workers(&plan);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    if (!plan.direct_mode) {
        status = sixel_parallel_finalize_palette(image,
                                                 plan.band_color_max,
                                                 band_count);
        if (SIXEL_FAILED(status)) {
            goto end;
        }
        sixel_parallel_apply_final_palette(image, prescan);
    } else {
        image->ncolors = 0;
    }
    if (used_parallel != NULL) {
        *used_parallel = 1;
    }
    status = SIXEL_OK;
end:
    if (prescan != NULL) {
        sixel_prescan_destroy(prescan, allocator);
    }
    if (plan.band_color_max != NULL) {
        sixel_allocator_free(allocator, plan.band_color_max);
    }
    return status;
}

#else /* !SIXEL_ENABLE_THREADS */

int sixel_decoder_parallel_resolve_threads(void)
{
    return 1;
}

#endif /* SIXEL_ENABLE_THREADS */

SIXELSTATUS sixel_decode_raw_parallel(unsigned char *p,
                                      int len,
                                      image_buffer_t *image,
                                      sixel_allocator_t *allocator,
                                      int *used_parallel)
{
#if SIXEL_ENABLE_THREADS
    return sixel_parallel_decode_internal(p,
                                          len,
                                          image,
                                          allocator,
                                          1,
                                          used_parallel);
#else
    if (used_parallel != NULL) {
        *used_parallel = 0;
    }
    (void)p;
    (void)len;
    (void)image;
    (void)allocator;
    return SIXEL_OK;
#endif
}

SIXELSTATUS sixel_decode_direct_parallel(unsigned char *p,
                                         int len,
                                         image_buffer_t *image,
                                         sixel_allocator_t *allocator,
                                         int *used_parallel)
{
#if SIXEL_ENABLE_THREADS
    return sixel_parallel_decode_internal(p,
                                          len,
                                          image,
                                          allocator,
                                          4,
                                          used_parallel);
#else
    if (used_parallel != NULL) {
        *used_parallel = 0;
    }
    (void)p;
    (void)len;
    (void)image;
    (void)allocator;
    return SIXEL_OK;
#endif
}
