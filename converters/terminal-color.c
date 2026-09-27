/*
 * SPDX-License-Identifier: MIT
 *
 * img2sixel owns this parser; library code must never negotiate a terminal
 * color policy.  Only complete DCS replies can supply capability or policy
 * values.  Framing is independent of read boundaries and uses fixed memory.
 */

#include <string.h>

#include "terminal-color.h"

enum img2sixel_color_state {
    IMG2SIXEL_COLOR_GROUND,
    IMG2SIXEL_COLOR_ESCAPE,
    IMG2SIXEL_COLOR_DCS_HEADER,
    IMG2SIXEL_COLOR_DCS_BODY,
    IMG2SIXEL_COLOR_CSI,
    IMG2SIXEL_COLOR_OSC,
    IMG2SIXEL_COLOR_STRING
};

static void
img2sixel_color_start(img2sixel_color_parser_t *parser)
{
    parser->state = IMG2SIXEL_COLOR_DCS_HEADER;
    parser->header_size = 0u;
    parser->body_size = 0u;
    parser->payload_size = 0u;
    parser->header_overflow = 0;
    parser->body_overflow = 0;
    memset(parser->header_tail, 0, sizeof(parser->header_tail));
    memset(parser->body_tail, 0, sizeof(parser->body_tail));
}

/* DEC scanners saturate at 9999. Reserve that value so a clipped field
 * cannot become a valid target, definition or simple-RGB gamma limb. */
static int
img2sixel_color_number(char const *bytes, size_t size, unsigned long *out)
{
    unsigned long value;
    unsigned long digit;
    size_t i;

    value = 0ul;
    if (size == 0u) {
        return 0;
    }
    for (i = 0u; i < size; ++i) {
        if (bytes[i] < '0' || bytes[i] > '9') {
            return 0;
        }
        digit = (unsigned long)(bytes[i] - '0');
        if (value > (9998ul - digit) / 10ul) {
            return 0;
        }
        value = value * 10ul + digit;
    }
    *out = value;
    return 1;
}

/* The capability grammar permits empty C, P and W; no other field does. */
static int
img2sixel_color_params(char const *bytes, size_t size,
                      unsigned long *values, size_t *count,
                      size_t capacity, unsigned int empty_mask)
{
    size_t start;
    size_t end;

    *count = 0u;
    start = 0u;
    for (end = 0u; end <= size; ++end) {
        if (end != size && bytes[end] != ';') {
            continue;
        }
        if (*count == capacity) {
            return 0;
        }
        if (end == start && (empty_mask & (1u << *count)) != 0u) {
            values[*count] = 0ul;
        } else if (!img2sixel_color_number(bytes + start, end - start,
                                          values + *count)) {
            return 0;
        }
        ++*count;
        start = end + 1u;
    }
    return 1;
}

static int
img2sixel_color_other_target(char const *bytes, size_t size)
{
    size_t end;
    unsigned long target;

    for (end = 0u; end < size && bytes[end] != ';'; ++end) {
        /* Find only the first parameter, before validating this target. */
    }
    return img2sixel_color_number(bytes, end, &target) && target != 1ul;
}

static img2sixel_color_result_t
img2sixel_color_capability(img2sixel_color_parser_t *parser)
{
    unsigned long values[10];
    unsigned long gamma;
    size_t limb;
    size_t count;
    size_t size;

    if (memcmp(parser->header_tail, "./r", 3u) != 0) {
        return IMG2SIXEL_COLOR_PENDING;
    }
    size = parser->header_size;
    if (size < 3u || parser->header_overflow) {
        return IMG2SIXEL_COLOR_INVALID;
    }
    if (img2sixel_color_other_target(parser->header, size - 3u)) {
        return IMG2SIXEL_COLOR_PENDING;
    }
    if (!img2sixel_color_params(parser->header, size - 3u, values,
                               &count, 10u, (1u << 4) | (1u << 8) |
                               (1u << 9)) ||
        count < 4u || values[0] != 1ul ||
        values[1] > 1ul || values[2] > 1ul || values[3] > 1ul) {
        return IMG2SIXEL_COLOR_INVALID;
    }
    if (count == 4u || values[4] == 0ul) {
        values[4] = 3ul;
    }
    /*
     * Simple RGB carries three mandatory base-9999 gamma limbs, followed
     * by optional P/W. Validate even though this pilot has no transform.
     * Bound each Horner step before multiplying: unsigned long can be
     * 32-bit, and arbitrary valid limbs need more than 32 bits together.
     */
    if (values[4] == 1ul) {
        if (count < 8u || parser->payload_size != 0u) {
            return IMG2SIXEL_COLOR_INVALID;
        }
        gamma = 0ul;
        for (limb = 5u; limb < 8u; ++limb) {
            if (gamma > (2147483647ul - values[limb]) / 9999ul) {
                return IMG2SIXEL_COLOR_INVALID;
            }
            gamma = gamma * 9999ul + values[limb];
        }
        if (gamma == 0ul) {
            return IMG2SIXEL_COLOR_INVALID;
        }
        if (count < 9u || values[8] == 0ul) {
            values[8] = 1ul;
        }
        if (count < 10u || values[9] == 0ul) {
            values[9] = 1ul;
        }
        if (values[8] > 3ul || values[9] > 3ul) {
            return IMG2SIXEL_COLOR_INVALID;
        }
    } else if (count > 5u) {
        return IMG2SIXEL_COLOR_INVALID;
    }
    /* A=0, or both optional policies absent, requires the empty sRGB form. */
    if (values[1] == 0ul || (values[2] == 0ul && values[3] == 0ul)) {
        if (values[2] != 0ul || values[3] != 0ul || values[4] != 3ul ||
            parser->payload_size != 0u) {
            return IMG2SIXEL_COLOR_INVALID;
        }
    }
    if ((values[4] == 3ul || values[4] == 4ul || values[4] == 5ul) &&
        parser->payload_size != 0u) {
        return IMG2SIXEL_COLOR_INVALID;
    }
    /* Nonempty ICC bytes remain unvalidated: this pilot only drains them. */
    if (values[4] == 2ul && parser->payload_size == 0u) {
        return IMG2SIXEL_COLOR_INVALID;
    }
    parser->s2 = values[2];
    parser->s3 = values[3];
    parser->definition = values[4];
    if (values[1] == 0ul) {
        return IMG2SIXEL_COLOR_UNAVAILABLE;
    }
    if (values[4] != 3ul && values[4] != 4ul) {
        return IMG2SIXEL_COLOR_UNSUPPORTED;
    }
    return IMG2SIXEL_COLOR_AVAILABLE;
}

static img2sixel_color_result_t
img2sixel_color_policy(img2sixel_color_parser_t *parser)
{
    unsigned long status;
    unsigned long values[2];
    size_t count;
    size_t size;

    size = parser->header_size;
    if (size < 3u || memcmp(parser->header_tail + 1, "$r", 2u) != 0) {
        return IMG2SIXEL_COLOR_PENDING;
    }
    if (parser->header_overflow ||
        !img2sixel_color_number(parser->header, size - 2u, &status) ||
        status > 1ul) {
        return IMG2SIXEL_COLOR_INVALID;
    }
    if (status == 0ul) {
        return parser->payload_size == 0u ? IMG2SIXEL_COLOR_POLICY_FAILURE
                                         : IMG2SIXEL_COLOR_INVALID;
    }
    /* Other DECRQSS identifiers cannot satisfy this single pending query. */
    if (memcmp(parser->body_tail, "./p", 3u) != 0) {
        return IMG2SIXEL_COLOR_PENDING;
    }
    size = parser->body_size;
    if (parser->body_overflow) {
        return IMG2SIXEL_COLOR_INVALID;
    }
    if (img2sixel_color_other_target(parser->body, size - 3u)) {
        return IMG2SIXEL_COLOR_PENDING;
    }
    if (!img2sixel_color_params(parser->body, size - 3u, values,
                               &count, 2u, 0u) || count != 2u ||
        values[0] != 1ul || values[1] > 3ul) {
        return IMG2SIXEL_COLOR_INVALID;
    }
    parser->policy = values[1];
    return IMG2SIXEL_COLOR_SAVED_POLICY;
}

static void
img2sixel_color_finish(img2sixel_color_parser_t *parser)
{
    if (parser->state == IMG2SIXEL_COLOR_DCS_BODY) {
        parser->result = parser->query == IMG2SIXEL_COLOR_CAPABILITY
                       ? img2sixel_color_capability(parser)
                       : img2sixel_color_policy(parser);
    }
    parser->state = IMG2SIXEL_COLOR_GROUND;
}

static void
img2sixel_color_append(char *bytes, size_t *size, char *tail,
                      int *overflow, unsigned char byte)
{
    tail[0] = tail[1];
    tail[1] = tail[2];
    tail[2] = (char)byte;
    if (*size < 128u) {
        bytes[(*size)++] = (char)byte;
    } else {
        *overflow = 1;
    }
}

static void
img2sixel_color_byte(img2sixel_color_parser_t *parser, unsigned char byte)
{
    /* CAN/SUB cancel; ESC delays that decision because ESC \ is also ST. */
    if (byte == 0x18u || byte == 0x1au) {
        parser->state = IMG2SIXEL_COLOR_GROUND;
        return;
    }
    if (byte == 0x1bu) {
        parser->escaped_state = parser->state;
        parser->state = IMG2SIXEL_COLOR_ESCAPE;
        return;
    }
    if (byte == 0x9cu) {
        if (parser->state == IMG2SIXEL_COLOR_ESCAPE) {
            /* An unmatched ESC cancels the old string before this C1 ST. */
            parser->state = IMG2SIXEL_COLOR_GROUND;
        }
        img2sixel_color_finish(parser);
        return;
    }
    if (byte == 0x90u) {
        img2sixel_color_start(parser);
        return;
    }
    if (byte == 0x9bu || byte == 0x9du || byte == 0x98u ||
        byte == 0x9eu || byte == 0x9fu) {
        parser->state = byte == 0x9bu ? IMG2SIXEL_COLOR_CSI
                      : byte == 0x9du ? IMG2SIXEL_COLOR_OSC
                                      : IMG2SIXEL_COLOR_STRING;
        return;
    }
    switch (parser->state) {
    case IMG2SIXEL_COLOR_ESCAPE:
        parser->state = IMG2SIXEL_COLOR_GROUND;
        switch (byte) {
        case '\\':
            parser->state = parser->escaped_state;
            img2sixel_color_finish(parser);
            break;
        case 'P':
            img2sixel_color_start(parser);
            break;
        case '[':
            parser->state = IMG2SIXEL_COLOR_CSI;
            break;
        case ']':
            parser->state = IMG2SIXEL_COLOR_OSC;
            break;
        case 'X':
        case '^':
        case '_':
            parser->state = IMG2SIXEL_COLOR_STRING;
            break;
        default:
            break;
        }
        break;
    case IMG2SIXEL_COLOR_DCS_HEADER:
        img2sixel_color_append(parser->header, &parser->header_size,
                              parser->header_tail, &parser->header_overflow,
                              byte);
        if (byte >= 0x40u && byte <= 0x7eu) {
            parser->state = IMG2SIXEL_COLOR_DCS_BODY;
        }
        break;
    case IMG2SIXEL_COLOR_DCS_BODY:
        img2sixel_color_append(parser->body, &parser->body_size,
                              parser->body_tail, &parser->body_overflow,
                              byte);
        /* Saturate instead of limiting or wrapping an unsupported payload. */
        if (parser->payload_size != (size_t)-1) {
            ++parser->payload_size;
        }
        break;
    case IMG2SIXEL_COLOR_CSI:
        if (byte >= 0x40u && byte <= 0x7eu) {
            parser->state = IMG2SIXEL_COLOR_GROUND;
        }
        break;
    case IMG2SIXEL_COLOR_OSC:
        if (byte == 0x07u) {
            parser->state = IMG2SIXEL_COLOR_GROUND;
        }
        break;
    default:
        break;
    }
}

void
img2sixel_color_init(img2sixel_color_parser_t *parser,
                    img2sixel_color_query_t query)
{
    memset(parser, 0, sizeof(*parser));
    parser->query = query;
}

img2sixel_color_result_t
img2sixel_color_feed(img2sixel_color_parser_t *parser,
                    unsigned char const *bytes, size_t size,
                    size_t *consumed)
{
    *consumed = 0u;
    while (*consumed < size && parser->result == IMG2SIXEL_COLOR_PENDING) {
        img2sixel_color_byte(parser, bytes[(*consumed)++]);
    }
    return parser->result;
}

int
img2sixel_color_incomplete(img2sixel_color_parser_t const *parser)
{
    return parser->state != IMG2SIXEL_COLOR_GROUND;
}

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
