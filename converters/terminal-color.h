/*
 * SPDX-License-Identifier: MIT
 *
 * Pure, incremental recognition of img2sixel terminal color replies.
 * This private CLI module performs no I/O and has no library dependency.
 */

#ifndef IMG2SIXEL_TERMINAL_COLOR_H
#define IMG2SIXEL_TERMINAL_COLOR_H

#include <stddef.h>

typedef enum img2sixel_color_query {
    IMG2SIXEL_COLOR_CAPABILITY,
    IMG2SIXEL_COLOR_POLICY
} img2sixel_color_query_t;

typedef enum img2sixel_color_result {
    IMG2SIXEL_COLOR_PENDING,
    IMG2SIXEL_COLOR_AVAILABLE,
    IMG2SIXEL_COLOR_UNAVAILABLE,
    IMG2SIXEL_COLOR_UNSUPPORTED,
    IMG2SIXEL_COLOR_SAVED_POLICY,
    IMG2SIXEL_COLOR_POLICY_FAILURE,
    IMG2SIXEL_COLOR_INVALID
} img2sixel_color_result_t;

/*
 * Fixed storage bounds the header, not the unsupported payload.  Payloads
 * are drained through ST without allocation or a length limit.  The caller
 * owns its absolute I/O deadline and must not treat PENDING as a fallback.
 * Fields other than result/s2/s3/definition/policy are parser internals.
 */
typedef struct img2sixel_color_parser {
    img2sixel_color_query_t query;
    img2sixel_color_result_t result;
    unsigned long s2;
    unsigned long s3;
    unsigned long definition;
    unsigned long policy;
    unsigned int state;
    unsigned int escaped_state;
    char header[128];
    char body[128];
    char header_tail[3];
    char body_tail[3];
    size_t header_size;
    size_t body_size;
    size_t payload_size;
    int header_overflow;
    int body_overflow;
} img2sixel_color_parser_t;

void
img2sixel_color_init(img2sixel_color_parser_t *parser,
                    img2sixel_color_query_t query);

/*
 * Consume through the first complete matching reply and latch its result.
 * Unrelated replies are consumed but never searched for matching suffixes.
 * This reader accepts 7-bit and C1 framing. CAN/SUB cancel an open control;
 * ESC followed by anything except ST cancels it and starts a new escape.
 * Thus a fresh ESC P can recover within a cancelled string. This is the
 * CLI reader contract, not an assertion about the receiver's own parser.
 * consumed locates the first unread byte, including in concatenated input.
 * Initialize again only when a new, correlated request is actually allowed.
 */
img2sixel_color_result_t
img2sixel_color_feed(img2sixel_color_parser_t *parser,
                    unsigned char const *bytes,
                    size_t size,
                    size_t *consumed);

/* Nonzero means a control sequence is still open, not a complete reply. */
int
img2sixel_color_incomplete(img2sixel_color_parser_t const *parser);

#endif /* IMG2SIXEL_TERMINAL_COLOR_H */

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
