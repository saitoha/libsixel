/*
 * SPDX-License-Identifier: MIT
 *
 * Private, synchronous terminal transport for the CLI color protocol.
 * No library reader may run while a phase owns terminal input/termios.
 */

#ifndef IMG2SIXEL_TERMINAL_COLOR_TTY_H
#define IMG2SIXEL_TERMINAL_COLOR_TTY_H

#include "terminal-color.h"

typedef struct img2sixel_color_tty img2sixel_color_tty_t;
typedef int (*img2sixel_color_cancel_t)(void *context);

typedef enum img2sixel_color_io {
    IMG2SIXEL_COLOR_IO_OK,
    IMG2SIXEL_COLOR_IO_UNAVAILABLE,
    IMG2SIXEL_COLOR_IO_SILENT,
    IMG2SIXEL_COLOR_IO_PARTIAL,
    IMG2SIXEL_COLOR_IO_TIMEOUT,
    IMG2SIXEL_COLOR_IO_CANCELLED,
    IMG2SIXEL_COLOR_IO_CHANGED,
    IMG2SIXEL_COLOR_IO_ERROR
} img2sixel_color_io_t;

typedef struct img2sixel_color_exchange {
    img2sixel_color_parser_t parser;
    size_t request_written;
    int received;
} img2sixel_color_exchange_t;

/*
 * Open a distinct nonblocking description of output_fd's own terminal.
 * Never change output_fd's flags and never substitute an unrelated /dev/tty.
 * Unsupported platforms return UNAVAILABLE with *tty == NULL.
 */
img2sixel_color_io_t
img2sixel_color_tty_open(img2sixel_color_tty_t **tty, int output_fd,
                        img2sixel_color_cancel_t cancel, void *context);

/*
 * Recheck output identity and foreground ownership at every phase. Begin
 * borrows termios and masks job-control stops; end restores both. End MUST
 * run before calling the synchronous encoder, and again on error cleanup.
 */
img2sixel_color_io_t
img2sixel_color_tty_begin(img2sixel_color_tty_t *tty);

img2sixel_color_io_t
img2sixel_color_tty_end(img2sixel_color_tty_t *tty);

/*
 * Each exchange shares one absolute deadline across its write and reads.
 * SILENT means a complete request was written and zero bytes received.
 * Any timeout or loss of correlation permanently disables further queries
 * on this context. Only write-only recovery remains possible. The caller
 * must also call abandon after a semantically wrong policy confirmation.
 */
img2sixel_color_io_t
img2sixel_color_tty_query(img2sixel_color_tty_t *tty,
                         img2sixel_color_query_t query, int timeout_ms,
                         img2sixel_color_exchange_t *exchange);

void
img2sixel_color_tty_abandon(img2sixel_color_tty_t *tty);

/*
 * Bounded raw control write. Recovery ignores cancellation and the query
 * latch, but still requires the same foreground terminal and a deadline.
 * written reports a possible partial control sequence for caller cleanup.
 */
img2sixel_color_io_t
img2sixel_color_tty_write(img2sixel_color_tty_t *tty,
                         char const *bytes, size_t size, int timeout_ms,
                         int recovery, size_t *written);

/* End an outstanding phase, close the private fd, and release the handle. */
img2sixel_color_io_t
img2sixel_color_tty_close(img2sixel_color_tty_t *tty);

#endif /* IMG2SIXEL_TERMINAL_COLOR_TTY_H */
