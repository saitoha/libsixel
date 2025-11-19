#include "config.h"

#include <limits.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <sixel.h>

#include "allocator.h"
#include "decoder-prescan.h"
#include "output.h"

#define SIXEL_PRESCAN_INITIAL_BANDS 8
#define SIXEL_PRESCAN_MAX_REPEAT    0xffff

#define SIXEL_PRESCAN_RGBA(r, g, b, a) \
    (((uint32_t)(r) << 24) | ((uint32_t)(g) << 16) | \
     ((uint32_t)(b) << 8) | ((uint32_t)(a)))

#define SIXEL_PRESCAN_PALVAL(n, a, m) \
    (((n) * (a) + ((m) / 2)) / (m))

#define SIXEL_PRESCAN_XRGB(r, g, b) \
    SIXEL_PRESCAN_RGBA(SIXEL_PRESCAN_PALVAL((r), 255, 100), \
                       SIXEL_PRESCAN_PALVAL((g), 255, 100), \
                       SIXEL_PRESCAN_PALVAL((b), 255, 100), \
                       255)

/*
 * Parser state that mirrors the serial decoder but skips actual rendering.
 * The "state" member is copied into the band table so that each worker can
 * resume decoding from the exact same FSM state.
 */
typedef struct sixel_prescan_context {
    sixel_prescan_band_state_t state;
    int max_x;
    int max_y;
    int saw_draw;
} sixel_prescan_context_t;

static const uint32_t sixel_prescan_default_palette[16] = {
    SIXEL_PRESCAN_XRGB(0, 0, 0),
    SIXEL_PRESCAN_XRGB(20, 20, 80),
    SIXEL_PRESCAN_XRGB(80, 13, 13),
    SIXEL_PRESCAN_XRGB(20, 80, 20),
    SIXEL_PRESCAN_XRGB(80, 20, 80),
    SIXEL_PRESCAN_XRGB(20, 80, 80),
    SIXEL_PRESCAN_XRGB(80, 80, 20),
    SIXEL_PRESCAN_XRGB(53, 53, 53),
    SIXEL_PRESCAN_XRGB(26, 26, 26),
    SIXEL_PRESCAN_XRGB(33, 33, 60),
    SIXEL_PRESCAN_XRGB(60, 26, 26),
    SIXEL_PRESCAN_XRGB(33, 60, 33),
    SIXEL_PRESCAN_XRGB(60, 33, 60),
    SIXEL_PRESCAN_XRGB(33, 60, 60),
    SIXEL_PRESCAN_XRGB(60, 60, 33),
    SIXEL_PRESCAN_XRGB(80, 80, 80)
};

static uint32_t
sixel_prescan_hls_to_rgba(int hue, int lum, int sat)
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
        (1.0 - (lum > 50 ? (2 * (lum / 100.0) - 1.0):
                - (2 * (lum / 100.0) - 1.0))) / 2.0;
    min = lum - sat *
        (1.0 - (lum > 50 ? (2 * (lum / 100.0) - 1.0):
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

    return SIXEL_PRESCAN_RGBA(r, g, b, 255);
}

static void
sixel_prescan_palette_init(uint32_t *palette)
{
    int i;
    int idx;
    int r;
    int g;
    int b;
    int remaining;

    for (i = 0; i < 16; ++i) {
        palette[i] = sixel_prescan_default_palette[i];
    }

    idx = 16;
    for (r = 0; r < 6; ++r) {
        for (g = 0; g < 6; ++g) {
            for (b = 0; b < 6; ++b) {
                palette[idx++] = SIXEL_PRESCAN_RGBA(r * 51,
                                                    g * 51,
                                                    b * 51,
                                                    255);
            }
        }
    }

    for (i = 0; i < 24; ++i) {
        palette[idx++] = SIXEL_PRESCAN_RGBA(i * 11,
                                            i * 11,
                                            i * 11,
                                            255);
    }

    /*
     * Fill any remaining slots with opaque white. Using a bounded
     * loop on a precomputed count avoids the aggressive-loop
     * diagnostic seen with post-increment indexing on some GCC
     * configurations.
     */
    remaining = SIXEL_PRESCAN_PALETTE_MAX - idx;
    if (remaining > 0) {
        for (i = 0; i < remaining; ++i) {
            palette[idx + i] = SIXEL_PRESCAN_RGBA(255, 255, 255, 255);
        }
    }
}

static SIXELSTATUS
sixel_prescan_context_init(sixel_prescan_context_t *context)
{
    SIXELSTATUS status = SIXEL_FALSE;

    memset(context, 0, sizeof(*context));
    context->state.state = SIXEL_PRESCAN_PS_GROUND;
    context->state.pos_x = 0;
    context->state.pos_y = 0;
    context->state.repeat_count = 1;
    context->state.color_index = 15;
    context->state.bgindex = (-1);
    context->state.attributed_pan = 2;
    context->state.attributed_pad = 1;
    context->state.attributed_ph = 0;
    context->state.attributed_pv = 0;
    context->state.param = 0;
    context->state.nparams = 0;
    context->state.p2_background = 2;
    context->state.par_num = 1;
    context->state.par_den = 1;
    context->max_x = 0;
    context->max_y = 0;
    context->saw_draw = 0;
    sixel_prescan_palette_init(context->state.palette);

    status = SIXEL_OK;

    return status;
}

static SIXELSTATUS
sixel_prescan_safe_addition(sixel_prescan_band_state_t *state,
                            unsigned char value)
{
    SIXELSTATUS status = SIXEL_FALSE;
    int digit;

    digit = value - '0';
    if ((state->param > INT_MAX / 10) ||
            (digit > INT_MAX - state->param * 10)) {
        status = SIXEL_BAD_INTEGER_OVERFLOW;
        sixel_helper_set_additional_message(
            "sixel_prescan_safe_addition: integer overflow detected.");
        goto end;
    }
    state->param = state->param * 10 + digit;
    status = SIXEL_OK;

end:
    return status;
}

static void
sixel_prescan_disable_parallel(sixel_prescan_t *prescan, unsigned int flag)
{
    prescan->flags |= flag;
}

/*
 * Grow the arrays that store band metadata. The prescan makes a single pass,
 * so doubling the capacity keeps reallocations infrequent even for long
 * images.
 */
static SIXELSTATUS
sixel_prescan_ensure_capacity(sixel_prescan_t *prescan,
                              int required,
                              sixel_allocator_t *allocator)
{
    SIXELSTATUS status = SIXEL_FALSE;
    int new_capacity;
    size_t bytes;
    size_t *offsets;
    sixel_prescan_band_state_t *states;

    if (required <= prescan->band_capacity) {
        return SIXEL_OK;
    }

    if (prescan->band_capacity == 0) {
        new_capacity = SIXEL_PRESCAN_INITIAL_BANDS;
    } else {
        new_capacity = prescan->band_capacity;
    }
    while (new_capacity < required) {
        new_capacity *= 2;
    }

    bytes = (size_t)new_capacity * sizeof(size_t);
    offsets = (size_t *)sixel_allocator_realloc(allocator,
                                                prescan->band_start_offsets,
                                                bytes);
    if (offsets == NULL) {
        sixel_helper_set_additional_message(
            "sixel_prescan: realloc for start offsets failed.");
        status = SIXEL_BAD_ALLOCATION;
        goto end;
    }
    prescan->band_start_offsets = offsets;

    offsets = (size_t *)sixel_allocator_realloc(allocator,
                                                prescan->band_end_offsets,
                                                bytes);
    if (offsets == NULL) {
        sixel_helper_set_additional_message(
            "sixel_prescan: realloc for end offsets failed.");
        status = SIXEL_BAD_ALLOCATION;
        goto end;
    }
    prescan->band_end_offsets = offsets;

    bytes = (size_t)new_capacity * sizeof(sixel_prescan_band_state_t);
    states = (sixel_prescan_band_state_t *)sixel_allocator_realloc(
        allocator,
        prescan->band_states,
        bytes);
    if (states == NULL) {
        sixel_helper_set_additional_message(
            "sixel_prescan: realloc for band states failed.");
        status = SIXEL_BAD_ALLOCATION;
        goto end;
    }
    prescan->band_states = states;
    prescan->band_capacity = new_capacity;
    status = SIXEL_OK;

end:
    return status;
}

static void
sixel_prescan_store_snapshot(sixel_prescan_t *prescan,
                             sixel_prescan_context_t *context,
                             size_t start_offset)
{
    int index;

    index = prescan->band_count;
    prescan->band_start_offsets[index] = start_offset;
    prescan->band_end_offsets[index] = start_offset;
    prescan->band_states[index] = context->state;
}

static SIXELSTATUS
sixel_prescan_append_band(sixel_prescan_t *prescan,
                          sixel_prescan_context_t *context,
                          size_t start_offset,
                          sixel_allocator_t *allocator)
{
    SIXELSTATUS status = SIXEL_FALSE;

    status = sixel_prescan_ensure_capacity(prescan,
                                           prescan->band_count + 1,
                                           allocator);
    if (SIXEL_FAILED(status)) {
        goto end;
    }

    sixel_prescan_store_snapshot(prescan, context, start_offset);
    prescan->band_count += 1;
    status = SIXEL_OK;

end:
    return status;
}

/*
 * Register a new band boundary when DECGNL is encountered. The byte offset of
 * the "-" command becomes the end offset of the previous band, and the next
 * band starts with the byte immediately after it.
 */
static SIXELSTATUS
sixel_prescan_start_new_band(sixel_prescan_t *prescan,
                             sixel_prescan_context_t *context,
                             size_t newline_offset,
                             sixel_allocator_t *allocator)
{
    SIXELSTATUS status = SIXEL_FALSE;
    size_t start_offset;

    if (prescan->band_count > 0) {
        prescan->band_end_offsets[prescan->band_count - 1] = newline_offset;
    }

    context->state.pos_x = 0;
    context->state.pos_y += 6;
    start_offset = newline_offset + 1;

    status = sixel_prescan_append_band(prescan,
                                       context,
                                       start_offset,
                                       allocator);

    return status;
}

static void
sixel_prescan_update_draw_bounds(sixel_prescan_context_t *context,
                                 int span_x,
                                 int span_y)
{
    if (context->max_x < span_x) {
        context->max_x = span_x;
    }
    if (context->max_y < span_y) {
        context->max_y = span_y;
    }
}

static void
sixel_prescan_finalize_dimensions(sixel_prescan_context_t *context,
                                  sixel_prescan_t *prescan)
{
    int width;
    int height;

    width = context->max_x + 1;
    if (width < context->state.attributed_ph) {
        width = context->state.attributed_ph;
    }

    height = context->max_y + 1;
    if (height < context->state.attributed_pv) {
        height = context->state.attributed_pv;
    }

    if (width < 0) {
        width = 0;
    }
    if (height < 0) {
        height = 0;
    }

    prescan->width = width;
    prescan->height = height;
}

static void
sixel_prescan_handle_ra(sixel_prescan_t *prescan,
                        sixel_prescan_context_t *context)
{
    sixel_prescan_band_state_t *state;
    int previous_ph;
    int previous_pv;

    state = &context->state;
    previous_ph = state->attributed_ph;
    previous_pv = state->attributed_pv;

    if (state->nparams > 0) {
        state->attributed_pad = state->params[0];
    }
    if (state->nparams > 1) {
        state->attributed_pan = state->params[1];
    }
    if (state->nparams > 2 && state->params[2] > 0) {
        state->attributed_ph = state->params[2];
    }
    if (state->nparams > 3 && state->params[3] > 0) {
        state->attributed_pv = state->params[3];
    }

    if (state->attributed_pan <= 0) {
        state->attributed_pan = 1;
    }
    if (state->attributed_pad <= 0) {
        state->attributed_pad = 1;
    }

    if (context->saw_draw &&
            (state->attributed_ph != previous_ph ||
             state->attributed_pv != previous_pv)) {
        sixel_prescan_disable_parallel(prescan,
                                       SIXEL_PRESCAN_FLAG_UNSAFE_GEOMETRY);
    }

    state->par_num = state->attributed_pan;
    state->par_den = state->attributed_pad;
    state->nparams = 0;
    state->param = 0;
}

static void
sixel_prescan_store_color(sixel_prescan_t *prescan,
                          sixel_prescan_context_t *context,
                          uint32_t color)
{
    sixel_prescan_band_state_t *state;

    state = &context->state;
    if (state->color_index < 0) {
        state->color_index = 0;
    }
    if (state->color_index >= SIXEL_PRESCAN_PALETTE_MAX) {
        sixel_prescan_disable_parallel(prescan,
                                       SIXEL_PRESCAN_FLAG_UNSUPPORTED_COLOR);
        return;
    }
    state->palette[state->color_index] = color;
}

static void
sixel_prescan_mark_draw(sixel_prescan_context_t *context)
{
    context->saw_draw = 1;
}

/*
 * Execute the lightweight prescan. The function walks the SIXEL byte stream
 * once, records the parser state at the start of each 6-line band, and keeps
 * track of the final geometry without touching the output surface.
 */
SIXELSTATUS
sixel_prescan_run(unsigned char *p,
                  int len,
                  sixel_prescan_t **out_prescan,
                  sixel_allocator_t *allocator)
{
    SIXELSTATUS status = SIXEL_FALSE;
    sixel_prescan_t *prescan = NULL;
    sixel_prescan_context_t context;
    sixel_prescan_band_state_t *state;
    unsigned char *cursor;
    unsigned char *end;
    size_t offset;
    size_t consumed_len;
    int finished;
    int bits;
    int mask;
    int i;
    int j;
    int span;

    if (out_prescan == NULL || allocator == NULL) {
        status = SIXEL_BAD_ARGUMENT;
        goto end;
    }

    if (len < 0) {
        status = SIXEL_BAD_INPUT;
        goto end;
    }

    if (len > 0 && p == NULL) {
        status = SIXEL_BAD_ARGUMENT;
        goto end;
    }

    *out_prescan = NULL;

    prescan = (sixel_prescan_t *)sixel_allocator_calloc(allocator,
                                                        1,
                                                        sizeof(*prescan));
    if (prescan == NULL) {
        sixel_helper_set_additional_message(
            "sixel_prescan_run: allocation failed.");
        status = SIXEL_BAD_ALLOCATION;
        goto end;
    }

    status = sixel_prescan_context_init(&context);
    if (SIXEL_FAILED(status)) {
        goto end;
    }

    status = sixel_prescan_append_band(prescan, &context, 0, allocator);
    if (SIXEL_FAILED(status)) {
        goto end;
    }

    cursor = p;
    end = p;
    if (len > 0) {
        end = p + len;
    }
    offset = 0;
    finished = 0;
    consumed_len = (size_t)len;
    state = &context.state;

    while (!finished && cursor < end) {
        switch (state->state) {
        case SIXEL_PRESCAN_PS_GROUND:
            switch (*cursor) {
            case 0x1b:
                state->state = SIXEL_PRESCAN_PS_ESC;
                break;
            case 0x90:
                state->state = SIXEL_PRESCAN_PS_DCS;
                break;
            case 0x9c:
                finished = 1;
                consumed_len = offset + 1;
                break;
            default:
                break;
            }
            break;
        case SIXEL_PRESCAN_PS_ESC:
            switch (*cursor) {
            case '\\':
            case 0x9c:
                finished = 1;
                consumed_len = offset + 1;
                break;
            case 'P':
                state->param = -1;
                state->state = SIXEL_PRESCAN_PS_DCS;
                break;
            default:
                break;
            }
            break;
        case SIXEL_PRESCAN_PS_DCS:
            switch (*cursor) {
            case 0x1b:
                state->state = SIXEL_PRESCAN_PS_ESC;
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
                if (state->param < 0) {
                    state->param = 0;
                }
                status = sixel_prescan_safe_addition(state, *cursor);
                if (SIXEL_FAILED(status)) {
                    goto end;
                }
                break;
            case ';':
                if (state->param < 0) {
                    state->param = 0;
                }
                if (state->nparams < DECSIXEL_PARAMS_MAX) {
                    state->params[state->nparams++] = state->param;
                }
                state->param = 0;
                break;
            case 'q':
                if (state->param >= 0 &&
                        state->nparams < DECSIXEL_PARAMS_MAX) {
                    state->params[state->nparams++] = state->param;
                }
                if (state->nparams > 0) {
                    switch (state->params[0]) {
                    case 0:
                    case 1:
                        state->attributed_pad = 2;
                        break;
                    case 2:
                        state->attributed_pad = 5;
                        break;
                    case 3:
                    case 4:
                        state->attributed_pad = 4;
                        break;
                    case 5:
                    case 6:
                        state->attributed_pad = 3;
                        break;
                    case 7:
                    case 8:
                        state->attributed_pad = 2;
                        break;
                    case 9:
                        state->attributed_pad = 1;
                        break;
                    default:
                        state->attributed_pad = 2;
                        break;
                    }
                }
                if (state->nparams > 1) {
                    state->p2_background = state->params[1];
                    if (state->p2_background != 1) {
                        state->p2_background = 2;
                    }
                }
                if (state->nparams > 2) {
                    if (state->params[2] == 0) {
                        state->params[2] = 10;
                    }
                    state->attributed_pan =
                        state->attributed_pan * state->params[2] / 10;
                    state->attributed_pad =
                        state->attributed_pad * state->params[2] / 10;
                    if (state->attributed_pan <= 0) {
                        state->attributed_pan = 1;
                    }
                    if (state->attributed_pad <= 0) {
                        state->attributed_pad = 1;
                    }
                }
                state->nparams = 0;
                state->param = 0;
                state->state = SIXEL_PRESCAN_PS_DECSIXEL;
                break;
            default:
                break;
            }
            break;
        case SIXEL_PRESCAN_PS_DECSIXEL:
            switch (*cursor) {
            case 0x1b:
                state->state = SIXEL_PRESCAN_PS_ESC;
                break;
            case '"':
                state->param = 0;
                state->nparams = 0;
                state->state = SIXEL_PRESCAN_PS_DECGRA;
                break;
            case '!':
                state->param = 0;
                state->nparams = 0;
                state->state = SIXEL_PRESCAN_PS_DECGRI;
                break;
            case '#':
                state->param = 0;
                state->nparams = 0;
                state->state = SIXEL_PRESCAN_PS_DECGCI;
                break;
            case '$':
                state->pos_x = 0;
                break;
            case '-':
                status = sixel_prescan_start_new_band(prescan,
                                                      &context,
                                                      offset,
                                                      allocator);
                if (SIXEL_FAILED(status)) {
                    goto end;
                }
                state = &context.state;
                break;
            default:
                if (*cursor >= '?' && *cursor <= '~') {
                    bits = *cursor - '?';
                    if (state->pos_x < 0 || state->pos_y < 0) {
                        status = SIXEL_BAD_INPUT;
                        goto end;
                    }
                    if (bits == 0) {
                        state->pos_x += state->repeat_count;
                    } else {
                        mask = 0x01;
                        if (state->repeat_count <= 1) {
                            for (i = 0; i < 6; ++i) {
                                if ((bits & mask) != 0) {
                                    sixel_prescan_mark_draw(&context);
                                    sixel_prescan_update_draw_bounds(
                                        &context,
                                        state->pos_x,
                                        state->pos_y + i);
                                }
                                mask <<= 1;
                            }
                            state->pos_x += 1;
                        } else {
                            for (i = 0; i < 6; ++i) {
                                if ((bits & mask) != 0) {
                                    int c;

                                    sixel_prescan_mark_draw(&context);
                                    c = mask << 1;
                                    span = 1;
                                    for (j = 1; (i + j) < 6; ++j) {
                                        if ((bits & c) == 0) {
                                            break;
                                        }
                                        c <<= 1;
                                        span += 1;
                                    }
                                    sixel_prescan_update_draw_bounds(
                                        &context,
                                        state->pos_x +
                                            state->repeat_count - 1,
                                        state->pos_y + i + span - 1);
                                    i += (span - 1);
                                    mask <<= (span - 1);
                                }
                                mask <<= 1;
                            }
                            state->pos_x += state->repeat_count;
                        }
                    }
                    state->repeat_count = 1;
                }
                break;
            }
            break;
        case SIXEL_PRESCAN_PS_DECGRA:
            switch (*cursor) {
            case 0x1b:
                state->state = SIXEL_PRESCAN_PS_ESC;
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
                status = sixel_prescan_safe_addition(state, *cursor);
                if (SIXEL_FAILED(status)) {
                    goto end;
                }
                break;
            case ';':
                if (state->nparams < DECSIXEL_PARAMS_MAX) {
                    state->params[state->nparams++] = state->param;
                }
                state->param = 0;
                break;
            default:
                if (state->nparams < DECSIXEL_PARAMS_MAX) {
                    state->params[state->nparams++] = state->param;
                }
                state->param = 0;
                sixel_prescan_handle_ra(prescan, &context);
                state->state = SIXEL_PRESCAN_PS_DECSIXEL;
                continue;
                break;
            }
            break;
        case SIXEL_PRESCAN_PS_DECGRI:
            switch (*cursor) {
            case 0x1b:
                state->state = SIXEL_PRESCAN_PS_ESC;
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
                status = sixel_prescan_safe_addition(state, *cursor);
                if (SIXEL_FAILED(status)) {
                    goto end;
                }
                break;
            default:
                state->repeat_count = state->param;
                if (state->repeat_count == 0) {
                    state->repeat_count = 1;
                }
                if (state->repeat_count > SIXEL_PRESCAN_MAX_REPEAT) {
                    status = SIXEL_BAD_INPUT;
                    sixel_helper_set_additional_message(
                        "sixel_prescan: repeat parameter too large.");
                    goto end;
                }
                state->state = SIXEL_PRESCAN_PS_DECSIXEL;
                state->param = 0;
                state->nparams = 0;
                continue;
                break;
            }
            break;
        case SIXEL_PRESCAN_PS_DECGCI:
            switch (*cursor) {
            case 0x1b:
                state->state = SIXEL_PRESCAN_PS_ESC;
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
                status = sixel_prescan_safe_addition(state, *cursor);
                if (SIXEL_FAILED(status)) {
                    goto end;
                }
                break;
            case ';':
                if (state->nparams < DECSIXEL_PARAMS_MAX) {
                    state->params[state->nparams++] = state->param;
                }
                state->param = 0;
                break;
            default:
                if (state->nparams < DECSIXEL_PARAMS_MAX) {
                    state->params[state->nparams++] = state->param;
                }
                state->param = 0;
                if (state->nparams > 0) {
                    state->color_index = state->params[0];
                    if (state->color_index < 0) {
                        state->color_index = 0;
                    } else if (state->color_index >=
                            SIXEL_PALETTE_MAX_DECODER) {
                        state->color_index = SIXEL_PALETTE_MAX_DECODER - 1;
                    }
                }
                if (state->nparams > 4) {
                    if (state->params[1] == 1) {
                        if (state->params[2] > 360) {
                            state->params[2] = 360;
                        }
                        if (state->params[3] > 100) {
                            state->params[3] = 100;
                        }
                        if (state->params[4] > 100) {
                            state->params[4] = 100;
                        }
                        sixel_prescan_store_color(
                            prescan,
                            &context,
                            sixel_prescan_hls_to_rgba(state->params[2],
                                                      state->params[3],
                                                      state->params[4]));
                    } else if (state->params[1] == 2) {
                        if (state->params[2] > 100) {
                            state->params[2] = 100;
                        }
                        if (state->params[3] > 100) {
                            state->params[3] = 100;
                        }
                        if (state->params[4] > 100) {
                            state->params[4] = 100;
                        }
                        sixel_prescan_store_color(
                            prescan,
                            &context,
                            SIXEL_PRESCAN_XRGB(state->params[2],
                                               state->params[3],
                                               state->params[4]));
                    }
                }
                state->state = SIXEL_PRESCAN_PS_DECSIXEL;
                continue;
                break;
            }
            break;
        default:
            break;
        }

        if (!finished) {
            cursor++;
            offset++;
        }
    }

    if (prescan->band_count > 0) {
        prescan->band_end_offsets[prescan->band_count - 1] = consumed_len;
    }

    sixel_prescan_finalize_dimensions(&context, prescan);
    prescan->final_state = context.state;

    *out_prescan = prescan;
    status = SIXEL_OK;
    prescan = NULL;

end:
    if (prescan != NULL) {
        sixel_prescan_destroy(prescan, allocator);
    }

    return status;
}

void
sixel_prescan_destroy(sixel_prescan_t *prescan,
                      sixel_allocator_t *allocator)
{
    if (prescan == NULL || allocator == NULL) {
        return;
    }

    sixel_allocator_free(allocator, prescan->band_start_offsets);
    sixel_allocator_free(allocator, prescan->band_end_offsets);
    sixel_allocator_free(allocator, prescan->band_states);
    sixel_allocator_free(allocator, prescan);
}
