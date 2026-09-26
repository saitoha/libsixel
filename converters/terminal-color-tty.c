/*
 * SPDX-License-Identifier: MIT
 *
 * CLI-owned I/O phases surround, but never overlap, synchronous encoding.
 * The library retains ownership of its existing OSC11/CPR readers during
 * encode. This module neither calls those readers nor changes their state.
 */

#if !defined(_POSIX_C_SOURCE)
# define _POSIX_C_SOURCE 200809L
#endif

#if defined(HAVE_CONFIG_H)
# include "config.h"
#endif

#include <stdlib.h>
#include "terminal-color-tty.h"

#if HAVE_CLI_COLOR_TTY
# include <errno.h>
# include <fcntl.h>
# include <signal.h>
# include <sys/types.h>
# include <sys/select.h>
# include <sys/stat.h>
# include <termios.h>
# include <time.h>
# include <unistd.h>
# if defined(CLOCK_MONOTONIC) && defined(O_NOCTTY) && defined(O_NONBLOCK)
#  define IMG2SIXEL_COLOR_TTY_POSIX 1
# endif
#endif

#ifndef IMG2SIXEL_COLOR_TTY_POSIX
# define IMG2SIXEL_COLOR_TTY_POSIX 0
#endif

#if IMG2SIXEL_COLOR_TTY_POSIX

struct img2sixel_color_tty {
    int fd;
    int output_fd;
    struct stat identity;
    struct termios saved_termios;
    sigset_t saved_mask;
    int mode_owned;
    int mask_owned;
    int abandoned;
    img2sixel_color_cancel_t cancel;
    void *cancel_context;
};

static int
img2sixel_color_tty_same(struct stat const *a, struct stat const *b)
{
    return S_ISCHR(a->st_mode) && S_ISCHR(b->st_mode) &&
        a->st_dev == b->st_dev && a->st_ino == b->st_ino &&
        a->st_rdev == b->st_rdev;
}

static int
img2sixel_color_tty_current(img2sixel_color_tty_t *tty)
{
    struct stat current;

    return tty != NULL && tty->fd >= 0 &&
        fstat(tty->output_fd, &current) == 0 &&
        img2sixel_color_tty_same(&tty->identity, &current) &&
        isatty(tty->output_fd) && tcgetpgrp(tty->fd) == getpgrp();
}

/* There is deliberately no wall-clock fallback for an I/O deadline. */
static int
img2sixel_color_tty_deadline(struct timespec *deadline, int timeout_ms)
{
    if (timeout_ms < 1 || timeout_ms > 10000 ||
        clock_gettime(CLOCK_MONOTONIC, deadline) != 0) {
        return 0;
    }
    deadline->tv_sec += timeout_ms / 1000;
    deadline->tv_nsec += (long)(timeout_ms % 1000) * 1000000L;
    if (deadline->tv_nsec >= 1000000000L) {
        ++deadline->tv_sec;
        deadline->tv_nsec -= 1000000000L;
    }
    return 1;
}

static img2sixel_color_io_t
img2sixel_color_tty_wait_masked(img2sixel_color_tty_t *tty, int writing,
    int recovery, struct timespec const *deadline, sigset_t const *mask)
{
    struct timespec now;
    struct timespec timeout;
    fd_set ready;
    time_t seconds;
    long nanoseconds;
    int result;

    for (;;) {
        if (!img2sixel_color_tty_current(tty)) {
            return IMG2SIXEL_COLOR_IO_CHANGED;
        }
        if (!recovery && tty->cancel != NULL &&
            tty->cancel(tty->cancel_context)) {
            return IMG2SIXEL_COLOR_IO_CANCELLED;
        }
        if (clock_gettime(CLOCK_MONOTONIC, &now) != 0) {
            return IMG2SIXEL_COLOR_IO_ERROR;
        }
        seconds = deadline->tv_sec - now.tv_sec;
        nanoseconds = deadline->tv_nsec - now.tv_nsec;
        if (nanoseconds < 0) {
            --seconds;
            nanoseconds += 1000000000L;
        }
        if (seconds < 0 || (seconds == 0 && nanoseconds == 0)) {
            return IMG2SIXEL_COLOR_IO_TIMEOUT;
        }
        timeout.tv_sec = seconds;
        timeout.tv_nsec = nanoseconds;
        FD_ZERO(&ready);
        FD_SET(tty->fd, &ready);
        result = pselect(tty->fd + 1, writing ? NULL : &ready,
                         writing ? &ready : NULL, NULL, &timeout, mask);
        if (result > 0) {
            if (!img2sixel_color_tty_current(tty)) {
                return IMG2SIXEL_COLOR_IO_CHANGED;
            }
            if (!recovery && tty->cancel != NULL &&
                tty->cancel(tty->cancel_context)) {
                return IMG2SIXEL_COLOR_IO_CANCELLED;
            }
            if (clock_gettime(CLOCK_MONOTONIC, &now) != 0) {
                return IMG2SIXEL_COLOR_IO_ERROR;
            }
            if (now.tv_sec > deadline->tv_sec ||
                (now.tv_sec == deadline->tv_sec &&
                 now.tv_nsec >= deadline->tv_nsec)) {
                return IMG2SIXEL_COLOR_IO_TIMEOUT;
            }
            return IMG2SIXEL_COLOR_IO_OK;
        }
        if (result < 0 && errno != EINTR) {
            return IMG2SIXEL_COLOR_IO_ERROR;
        }
        /* Recompute from the absolute deadline after every EINTR. */
    }
}

static img2sixel_color_io_t
img2sixel_color_tty_wait(img2sixel_color_tty_t *tty, int writing,
                         int recovery, struct timespec const *deadline)
{
    sigset_t blocked;
    sigset_t saved;
    img2sixel_color_io_t result;
    int saved_errno;

    /* Keep cancellation pending between checking its flag and waiting.
     * pselect restores the caller's mask atomically with the wait; passing
     * that mask also gives Hurd a signal wakeup port during its select RPCs.
     * Unrelated signals, including watchdog alarms, remain unblocked. */
    sigemptyset(&blocked);
    sigaddset(&blocked, SIGINT);
    sigaddset(&blocked, SIGTERM);
    if (sigprocmask(SIG_BLOCK, &blocked, &saved) != 0) {
        return IMG2SIXEL_COLOR_IO_ERROR;
    }
    result = img2sixel_color_tty_wait_masked(tty, writing, recovery,
                                           deadline, &saved);
    saved_errno = errno;
    if (sigprocmask(SIG_SETMASK, &saved, NULL) != 0) {
        return IMG2SIXEL_COLOR_IO_ERROR;
    }
    errno = saved_errno;
    return result;
}

static img2sixel_color_io_t
img2sixel_color_tty_send(img2sixel_color_tty_t *tty,
                         char const *bytes, size_t size, int recovery,
                         struct timespec const *deadline, size_t *written)
{
    img2sixel_color_io_t result;
    ssize_t count;

    *written = 0u;
    while (*written < size) {
        result = img2sixel_color_tty_wait(tty, 1, recovery, deadline);
        if (result != IMG2SIXEL_COLOR_IO_OK) {
            return result;
        }
        /* Readiness promises space for at least one byte. Hurd PTYs can
         * block a larger write that only partly fits despite O_NONBLOCK.
         * These are short control sequences, so keep each write within
         * that promise and check the absolute deadline between bytes. */
        count = write(tty->fd, bytes + *written, 1u);
        if (count > 0) {
            *written += (size_t)count;
        } else if (count == 0 ||
                   (errno != EAGAIN && errno != EWOULDBLOCK &&
                    errno != EINTR)) {
            return IMG2SIXEL_COLOR_IO_ERROR;
        }
    }
    return IMG2SIXEL_COLOR_IO_OK;
}

img2sixel_color_io_t
img2sixel_color_tty_open(img2sixel_color_tty_t **tty, int output_fd,
                        img2sixel_color_cancel_t cancel, void *context)
{
    img2sixel_color_tty_t *created;
    struct stat identity;
    struct stat opened;
    struct timespec now;
    char const *path;
    int fd;

    if (tty == NULL) {
        return IMG2SIXEL_COLOR_IO_ERROR;
    }
    *tty = NULL;
    if (!isatty(output_fd) || fstat(output_fd, &identity) != 0 ||
        clock_gettime(CLOCK_MONOTONIC, &now) != 0) {
        return IMG2SIXEL_COLOR_IO_UNAVAILABLE;
    }
    path = ttyname(output_fd);
    if (path == NULL) {
        return IMG2SIXEL_COLOR_IO_UNAVAILABLE;
    }
    fd = open(path, O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (fd < 0) {
        return IMG2SIXEL_COLOR_IO_UNAVAILABLE;
    }
    if (fd >= FD_SETSIZE || fstat(fd, &opened) != 0 ||
        !img2sixel_color_tty_same(&identity, &opened) ||
        tcgetpgrp(fd) != getpgrp() ||
        fcntl(fd, F_SETFD, FD_CLOEXEC) < 0) {
        close(fd);
        return IMG2SIXEL_COLOR_IO_UNAVAILABLE;
    }
    created = (img2sixel_color_tty_t *)calloc(1u, sizeof(*created));
    if (created == NULL) {
        close(fd);
        return IMG2SIXEL_COLOR_IO_ERROR;
    }
    created->fd = fd;
    created->output_fd = output_fd;
    created->identity = identity;
    created->cancel = cancel;
    created->cancel_context = context;
    *tty = created;
    return IMG2SIXEL_COLOR_IO_OK;
}

img2sixel_color_io_t
img2sixel_color_tty_begin(img2sixel_color_tty_t *tty)
{
    struct termios mode;
    sigset_t blocked;

    if (tty == NULL || tty->mode_owned || tty->mask_owned) {
        return IMG2SIXEL_COLOR_IO_ERROR;
    }
    if (!img2sixel_color_tty_current(tty)) {
        tty->abandoned = 1;
        return IMG2SIXEL_COLOR_IO_CHANGED;
    }
    /* A foreground transition can race the check. Blocking these signals
     * prevents a background read/write or tcsetattr from stopping cleanup. */
    sigemptyset(&blocked);
    sigaddset(&blocked, SIGTTIN);
    sigaddset(&blocked, SIGTTOU);
    if (sigprocmask(SIG_BLOCK, &blocked, &tty->saved_mask) != 0) {
        return IMG2SIXEL_COLOR_IO_ERROR;
    }
    tty->mask_owned = 1;
    if (tcgetattr(tty->fd, &tty->saved_termios) != 0) {
        (void)img2sixel_color_tty_end(tty);
        return IMG2SIXEL_COLOR_IO_ERROR;
    }
    mode = tty->saved_termios;
    mode.c_iflag &= ~(ISTRIP | INLCR | IGNCR | ICRNL | IXON | IXOFF | PARMRK);
    mode.c_lflag &= ~(ICANON | ECHO | ECHONL | IEXTEN);
    /* SUB is a protocol cancellation byte, normally also bound to Ctrl-Z.
     * VSUSP would signal and may flush input before the parser sees it.
     * Keep ISIG/SIGINT handling, but suspend keys are protocol input until
     * end. Each exchange/write has its own deadline, at most 10 seconds;
     * begin/end do not impose an overall deadline on a multi-exchange phase.
     * Restoring the complete saved termios restores VSUSP before encode. */
    mode.c_cc[VSUSP] = _POSIX_VDISABLE;
    mode.c_cc[VMIN] = 1;
    mode.c_cc[VTIME] = 0;
    /* Mark ownership before tcsetattr: even a failed call can need undo. */
    tty->mode_owned = 1;
    if (tcsetattr(tty->fd, TCSANOW, &mode) != 0) {
        (void)img2sixel_color_tty_end(tty);
        return IMG2SIXEL_COLOR_IO_ERROR;
    }
    return IMG2SIXEL_COLOR_IO_OK;
}

img2sixel_color_io_t
img2sixel_color_tty_end(img2sixel_color_tty_t *tty)
{
    img2sixel_color_io_t result;

    result = IMG2SIXEL_COLOR_IO_OK;
    if (tty == NULL) {
        return result;
    }
    if (tty->mode_owned) {
        if (tcsetattr(tty->fd, TCSANOW, &tty->saved_termios) != 0) {
            result = IMG2SIXEL_COLOR_IO_ERROR;
        } else {
            tty->mode_owned = 0;
        }
    }
    if (tty->mask_owned) {
        if (sigprocmask(SIG_SETMASK, &tty->saved_mask, NULL) != 0) {
            result = IMG2SIXEL_COLOR_IO_ERROR;
        } else {
            tty->mask_owned = 0;
        }
    }
    if (result != IMG2SIXEL_COLOR_IO_OK) {
        tty->abandoned = 1;
    }
    return result;
}

void
img2sixel_color_tty_abandon(img2sixel_color_tty_t *tty)
{
    if (tty != NULL) {
        tty->abandoned = 1;
    }
}

img2sixel_color_io_t
img2sixel_color_tty_query(img2sixel_color_tty_t *tty,
                         img2sixel_color_query_t query, int timeout_ms,
                         img2sixel_color_exchange_t *exchange)
{
    static char const capability[] = "\033[?1./r";
    static char const policy[] = "\033P$q1./p\033\\";
    struct timespec deadline;
    struct timespec immediate;
    sigset_t mask;
    fd_set pending;
    img2sixel_color_io_t result;
    char const *request;
    size_t request_size;
    size_t consumed;
    ssize_t count;
    unsigned char byte;

    if (exchange == NULL) {
        return IMG2SIXEL_COLOR_IO_ERROR;
    }
    img2sixel_color_init(&exchange->parser, query);
    exchange->request_written = 0u;
    exchange->received = 0;
    if (tty == NULL || !tty->mode_owned || tty->abandoned ||
        (query != IMG2SIXEL_COLOR_CAPABILITY &&
         query != IMG2SIXEL_COLOR_POLICY) ||
        !img2sixel_color_tty_deadline(&deadline, timeout_ms)) {
        return IMG2SIXEL_COLOR_IO_ERROR;
    }
    /* Queued bytes before a request cannot be its correlated reply. Leave
     * them untouched and stop, rather than reusing a stale saved policy. */
    FD_ZERO(&pending);
    FD_SET(tty->fd, &pending);
    immediate.tv_sec = 0;
    immediate.tv_nsec = 0;
    if (sigprocmask(SIG_BLOCK, NULL, &mask) != 0 ||
        pselect(tty->fd + 1, &pending, NULL, NULL, &immediate, &mask) != 0) {
        tty->abandoned = 1;
        return IMG2SIXEL_COLOR_IO_ERROR;
    }
    request = query == IMG2SIXEL_COLOR_CAPABILITY ? capability : policy;
    request_size = query == IMG2SIXEL_COLOR_CAPABILITY
        ? sizeof(capability) - 1u : sizeof(policy) - 1u;
    result = img2sixel_color_tty_send(tty, request, request_size, 0,
                                     &deadline, &exchange->request_written);
    while (result == IMG2SIXEL_COLOR_IO_OK) {
        result = img2sixel_color_tty_wait(tty, 0, 0, &deadline);
        if (result == IMG2SIXEL_COLOR_IO_TIMEOUT) {
            result = exchange->received ? IMG2SIXEL_COLOR_IO_PARTIAL
                                       : IMG2SIXEL_COLOR_IO_SILENT;
        }
        if (result != IMG2SIXEL_COLOR_IO_OK) {
            break;
        }
        /* A single byte avoids consuming any next response or user input
         * past ST. The parser owns bounded storage and the loop a deadline. */
        count = read(tty->fd, &byte, 1u);
        if (count == 1) {
            exchange->received = 1;
            if (img2sixel_color_feed(&exchange->parser, &byte, 1u,
                                     &consumed) !=
                IMG2SIXEL_COLOR_PENDING) {
                if (exchange->parser.result == IMG2SIXEL_COLOR_INVALID ||
                    exchange->parser.result == IMG2SIXEL_COLOR_POLICY_FAILURE) {
                    tty->abandoned = 1;
                }
                return IMG2SIXEL_COLOR_IO_OK;
            }
        } else if (count == 0 ||
                   (errno != EAGAIN && errno != EWOULDBLOCK &&
                    errno != EINTR)) {
            result = IMG2SIXEL_COLOR_IO_ERROR;
        }
    }
    tty->abandoned = 1;
    return result;
}

img2sixel_color_io_t
img2sixel_color_tty_write(img2sixel_color_tty_t *tty,
                         char const *bytes, size_t size, int timeout_ms,
                         int recovery, size_t *written)
{
    struct timespec deadline;
    img2sixel_color_io_t result;

    if (written == NULL) {
        return IMG2SIXEL_COLOR_IO_ERROR;
    }
    *written = 0u;
    if (tty == NULL || !tty->mode_owned || bytes == NULL ||
        (!recovery && tty->abandoned) ||
        !img2sixel_color_tty_deadline(&deadline, timeout_ms)) {
        return IMG2SIXEL_COLOR_IO_ERROR;
    }
    result = img2sixel_color_tty_send(tty, bytes, size, recovery,
                                     &deadline, written);
    if (result != IMG2SIXEL_COLOR_IO_OK) {
        tty->abandoned = 1;
    }
    return result;
}

img2sixel_color_io_t
img2sixel_color_tty_close(img2sixel_color_tty_t *tty)
{
    img2sixel_color_io_t result;

    result = img2sixel_color_tty_end(tty);
    if (tty != NULL) {
        if (close(tty->fd) != 0) {
            result = IMG2SIXEL_COLOR_IO_ERROR;
        }
        free(tty);
    }
    return result;
}

#else /* No supported nonblocking, foreground POSIX terminal I/O. */

img2sixel_color_io_t
img2sixel_color_tty_open(img2sixel_color_tty_t **tty, int output_fd,
                        img2sixel_color_cancel_t cancel, void *context)
{
    (void)output_fd;
    (void)cancel;
    (void)context;
    if (tty == NULL) {
        return IMG2SIXEL_COLOR_IO_ERROR;
    }
    *tty = NULL;
    return IMG2SIXEL_COLOR_IO_UNAVAILABLE;
}

img2sixel_color_io_t
img2sixel_color_tty_begin(img2sixel_color_tty_t *tty)
{
    (void)tty;
    return IMG2SIXEL_COLOR_IO_UNAVAILABLE;
}

img2sixel_color_io_t
img2sixel_color_tty_end(img2sixel_color_tty_t *tty)
{
    (void)tty;
    return IMG2SIXEL_COLOR_IO_OK;
}

void
img2sixel_color_tty_abandon(img2sixel_color_tty_t *tty)
{
    (void)tty;
}

img2sixel_color_io_t
img2sixel_color_tty_query(img2sixel_color_tty_t *tty,
                         img2sixel_color_query_t query, int timeout_ms,
                         img2sixel_color_exchange_t *exchange)
{
    (void)tty;
    (void)query;
    (void)timeout_ms;
    if (exchange != NULL) {
        img2sixel_color_init(&exchange->parser, query);
        exchange->request_written = 0u;
        exchange->received = 0;
    }
    return IMG2SIXEL_COLOR_IO_UNAVAILABLE;
}

img2sixel_color_io_t
img2sixel_color_tty_write(img2sixel_color_tty_t *tty,
                         char const *bytes, size_t size, int timeout_ms,
                         int recovery, size_t *written)
{
    (void)tty;
    (void)bytes;
    (void)size;
    (void)timeout_ms;
    (void)recovery;
    if (written != NULL) {
        *written = 0u;
    }
    return IMG2SIXEL_COLOR_IO_UNAVAILABLE;
}

img2sixel_color_io_t
img2sixel_color_tty_close(img2sixel_color_tty_t *tty)
{
    (void)tty;
    return IMG2SIXEL_COLOR_IO_OK;
}

#endif /* IMG2SIXEL_COLOR_TTY_POSIX */
