/*
 * SPDX-License-Identifier: MIT
 *
 * Finite PTY scripts exercise the real private CLI transport. The child
 * owns a new controlling terminal; the parent supplies literal replies.
 * Pipes synchronize exceptional paths without fixed sleeps. No library
 * TTY reader, terminal emulator, or installed converter is substituted.
 */
#ifndef _POSIX_C_SOURCE
# define _POSIX_C_SOURCE 200809L
#endif
#ifndef _XOPEN_SOURCE
# define _XOPEN_SOURCE 700
#endif
#if defined(__APPLE__) && !defined(_DARWIN_C_SOURCE)
# define _DARWIN_C_SOURCE 1
#endif
#if defined(HAVE_CONFIG_H)
# include "config.h"
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include "converters/terminal-color-tty.h"

#if HAVE_CLI_COLOR_PTY && HAVE_CLI_COLOR_TTY
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

#define COLOR_TTY_QUERY "\033[?1./r"
#define COLOR_TTY_POLICY "\033P$q1./p\033\\"
#define COLOR_TTY_REPLY "\033P1;1;1;1;4./r\033\\"
#define COLOR_TTY_RECOVER "\030\033\\\033[1;0./p"
/* The peer is another scheduled process, including inside CPU-limited CI.
 * Successful replies do not consume this budget. Timeout scripts still
 * assert an absolute bound below the independent two-second watchdog. */
#define COLOR_TTY_REPLY_TIMEOUT_MS 500

static volatile sig_atomic_t color_tty_cancelled;

static void
color_tty_signal(int number)
{
    if (number == SIGINT || number == SIGTERM) {
        color_tty_cancelled = 1;
    }
}

static int
color_tty_cancel(void *context)
{
    (void)context;
    return color_tty_cancelled != 0;
}

static int
color_tty_error(char const *message)
{
    fprintf(stderr, "CLI color TTY: %s (errno=%d)\n", message, errno);
    return 1;
}

static double
color_tty_now(void)
{
    struct timespec now;

    if (clock_gettime(CLOCK_MONOTONIC, &now) != 0) {
        return -1.0;
    }
    return (double)now.tv_sec + (double)now.tv_nsec / 1000000000.0;
}

/* The script's independent watchdog also bounds a broken production wait. */
static int
color_tty_ready(int fd, int writing)
{
    struct timeval timeout;
    fd_set ready;
    double deadline;
    double left;
    int result;

    deadline = color_tty_now() + 2.0;
    do {
        left = deadline - color_tty_now();
        if (left <= 0.0 || fd < 0 || fd >= FD_SETSIZE) {
            return 0;
        }
        timeout.tv_sec = (long)left;
        timeout.tv_usec = (long)((left - timeout.tv_sec) * 1000000.0);
        FD_ZERO(&ready);
        FD_SET(fd, &ready);
        result = select(fd + 1, writing ? NULL : &ready,
                        writing ? &ready : NULL, NULL, &timeout);
    } while (result < 0 && errno == EINTR);
    return result > 0;
}

static int
color_tty_send(int fd, char const *bytes, size_t size)
{
    ssize_t count;
    size_t sent;

    sent = 0u;
    while (sent < size) {
        if (!color_tty_ready(fd, 1)) {
            return 0;
        }
        count = write(fd, bytes + sent, size - sent);
        if (count > 0) {
            sent += (size_t)count;
        } else if (count == 0 ||
                   (errno != EINTR && errno != EAGAIN)) {
            return 0;
        }
    }
    return 1;
}

static int
color_tty_expect(int fd, char const *expected)
{
    char bytes[64];
    size_t received;
    size_t size;
    ssize_t count;

    size = strlen(expected);
    received = 0u;
    if (size > sizeof(bytes)) {
        return 0;
    }
    while (received < size) {
        if (!color_tty_ready(fd, 0)) {
            return 0;
        }
        count = read(fd, bytes + received, size - received);
        if (count > 0) {
            received += (size_t)count;
        } else if (count == 0 ||
                   (errno != EINTR && errno != EAGAIN)) {
            return 0;
        }
    }
    return memcmp(bytes, expected, size) == 0;
}

/* Compare actual fields, not uninitialized padding in libc structures. */
static int
color_tty_mode_equal(struct termios const *a, struct termios const *b,
                      int canonical_restored)
{
    tcflag_t expected_lflag;

    expected_lflag = a->c_lflag;
#if defined(__APPLE__) && defined(__MACH__) && defined(PENDIN)
    /* Darwin sets kernel-maintained PENDIN when ICANON returns, even with
     * an empty input queue. xnu/bsd/kern/tty.c, TIOCSETA (2026-09-17), ORs
     * that state with the requested flags. Do not flush/read input just to
     * clear it. Only that canonical transition changes this expectation. */
    if (canonical_restored && (a->c_lflag & ICANON) != 0) {
        expected_lflag |= PENDIN;
    }
#else
    (void)canonical_restored;
#endif
    return a->c_iflag == b->c_iflag && a->c_oflag == b->c_oflag &&
        a->c_cflag == b->c_cflag && expected_lflag == b->c_lflag &&
        memcmp(a->c_cc, b->c_cc, sizeof(a->c_cc)) == 0 &&
        cfgetispeed(a) == cfgetispeed(b) && cfgetospeed(a) == cfgetospeed(b);
}

static int
color_tty_restored(struct termios const *mode, sigset_t const *mask,
                    int flags, int canonical_restored)
{
    struct termios current;
    sigset_t current_mask;
    int number;

    if (tcgetattr(STDOUT_FILENO, &current) != 0) {
        return 0;
    }
    if (!color_tty_mode_equal(mode, &current, canonical_restored)) {
        fprintf(stderr, "termios changed: %lu/%lu %lu/%lu %lu/%lu %lu/%lu\n",
            (unsigned long)mode->c_iflag, (unsigned long)current.c_iflag,
            (unsigned long)mode->c_oflag, (unsigned long)current.c_oflag,
            (unsigned long)mode->c_cflag, (unsigned long)current.c_cflag,
            (unsigned long)mode->c_lflag, (unsigned long)current.c_lflag);
        return 0;
    }
    if (fcntl(STDOUT_FILENO, F_GETFL) != flags ||
        sigprocmask(SIG_BLOCK, NULL, &current_mask) != 0) {
        fprintf(stderr, "flags or signal mask query changed\n");
        return 0;
    }
    for (number = 1; number <= (int)(sizeof(sigset_t) * CHAR_BIT); ++number) {
        if (sigismember(mask, number) != sigismember(&current_mask, number)) {
            fprintf(stderr, "signal mask changed: %d\n", number);
            return 0;
        }
    }
    return 1;
}

static int
color_tty_background(img2sixel_color_tty_t *tty)
{
    int ready[2];
    int result;
    int status;
    pid_t group;
    sigset_t blocked;
    sigset_t saved;

    if (pipe(ready) != 0) {
        return 1;
    }
    group = fork();
    if (group == 0) {
        alarm(2);
        close(ready[0]);
        if (setpgid(0, 0) != 0 || !color_tty_send(ready[1], "g", 1u)) {
            _exit(1);
        }
        for (;;) {
            pause();
        }
    }
    close(ready[1]);
    if (group < 0) {
        close(ready[0]);
        return 1;
    }
    result = 1;
    if (color_tty_expect(ready[0], "g") &&
        tcsetpgrp(STDOUT_FILENO, group) == 0) {
        result = img2sixel_color_tty_begin(tty) != IMG2SIXEL_COLOR_IO_CHANGED;
    }
    /* Return the terminal before stopping the auxiliary foreground group. */
    sigemptyset(&blocked);
    sigaddset(&blocked, SIGTTOU);
    sigprocmask(SIG_BLOCK, &blocked, &saved);
    if (tcsetpgrp(STDOUT_FILENO, getpgrp()) != 0) {
        result = 1;
    }
    sigprocmask(SIG_SETMASK, &saved, NULL);
    kill(group, SIGKILL);
    waitpid(group, &status, 0);
    close(ready[0]);
    return result;
}

static int
color_tty_child(char const *name, int command, int report)
{
    img2sixel_color_tty_t *tty;
    img2sixel_color_exchange_t exchange;
    img2sixel_color_io_t result;
    struct termios original;
    struct termios active;
    sigset_t original_mask;
    struct sigaction action;
    struct timeval immediate;
    fd_set writable;
    size_t written;
    int fill_ready;
    int flags;
    int pipes[2];
    int output_copy;
    int other_master;
    int other_slave;
    int mode_changed;
    char byte;
    char *large;
    double started;

    tty = NULL;
    mode_changed = 0;
    color_tty_cancelled = 0;
    memset(&action, 0, sizeof(action));
    action.sa_handler = color_tty_signal;
    sigemptyset(&action.sa_mask);
    sigaction(SIGINT, &action, NULL);
    sigaction(SIGTERM, &action, NULL);
    sigaction(SIGUSR2, &action, NULL);
    /* A broken blocking-write implementation cannot hang the test suite. */
    alarm(2);
    flags = fcntl(STDOUT_FILENO, F_GETFL);
    if (tcgetattr(STDOUT_FILENO, &original) != 0 ||
        sigprocmask(SIG_BLOCK, NULL, &original_mask) != 0) {
        return color_tty_error("cannot snapshot terminal");
    }
    if (strcmp(name, "non-tty") == 0) {
        if (pipe(pipes) != 0) {
            return 1;
        }
        result = img2sixel_color_tty_open(&tty, pipes[1], NULL, NULL);
        flags = fcntl(pipes[1], F_GETFL);
        close(pipes[0]);
        close(pipes[1]);
        return result != IMG2SIXEL_COLOR_IO_UNAVAILABLE || tty != NULL ||
            (flags & O_NONBLOCK) != 0;
    }
    result = img2sixel_color_tty_open(&tty, STDOUT_FILENO,
                                     color_tty_cancel, NULL);
    if (strcmp(name, "unrelated") == 0) {
        return result != IMG2SIXEL_COLOR_IO_UNAVAILABLE || tty != NULL;
    }
    if (result != IMG2SIXEL_COLOR_IO_OK) {
        return color_tty_error("cannot open matching transport");
    }
    if (strcmp(name, "identity") == 0) {
        output_copy = dup(STDOUT_FILENO);
        other_master = posix_openpt(O_RDWR | O_NOCTTY | O_NONBLOCK);
        if (other_master < 0 || grantpt(other_master) != 0 ||
            unlockpt(other_master) != 0) {
            return 1;
        }
        other_slave = open(ptsname(other_master), O_RDWR | O_NOCTTY);
        if (other_slave < 0 || dup2(other_slave, STDOUT_FILENO) < 0) {
            return 1;
        }
        result = img2sixel_color_tty_begin(tty);
        dup2(output_copy, STDOUT_FILENO);
        close(output_copy);
        close(other_slave);
        close(other_master);
        if (result != IMG2SIXEL_COLOR_IO_CHANGED) {
            return color_tty_error("replacement stdout was accepted");
        }
        goto finish;
    }
    if (strcmp(name, "foreground") == 0) {
        if (color_tty_background(tty) != 0) {
            return color_tty_error("background terminal was accepted");
        }
        goto finish;
    }
    if (strcmp(name, "inactive") == 0) {
        if (img2sixel_color_tty_query(tty, IMG2SIXEL_COLOR_CAPABILITY,
                                     20, &exchange) !=
                IMG2SIXEL_COLOR_IO_ERROR ||
            img2sixel_color_tty_write(tty, "x", 1u, 20, 0, &written) !=
                IMG2SIXEL_COLOR_IO_ERROR || written != 0u) {
            return color_tty_error("inactive phase performed I/O");
        }
        goto finish;
    }
    if (img2sixel_color_tty_begin(tty) != IMG2SIXEL_COLOR_IO_OK ||
        tcgetattr(STDOUT_FILENO, &active) != 0 ||
        (active.c_lflag & (ICANON | ECHO)) != 0 ||
        (active.c_lflag & ISIG) != (original.c_lflag & ISIG) ||
        fcntl(STDOUT_FILENO, F_GETFL) != flags) {
        return color_tty_error("phase ownership or stdout flags");
    }
    mode_changed = 1;
    if (strcmp(name, "close-active") == 0) {
        goto finish;
    }
    if (strcmp(name, "stale") == 0) {
        if (!color_tty_send(report, "s", 1u) ||
            !color_tty_expect(command, "s")) {
            return 1;
        }
        result = img2sixel_color_tty_query(tty, IMG2SIXEL_COLOR_CAPABILITY,
                                          20, &exchange);
        if (result != IMG2SIXEL_COLOR_IO_ERROR ||
            exchange.request_written != 0u ||
            read(STDOUT_FILENO, &byte, 1u) != 1 || byte != '\033') {
            return color_tty_error("stale input adopted or discarded");
        }
        goto finish;
    }
    if (strcmp(name, "backpressure") == 0) {
        large = (char *)malloc(1024u * 1024u);
        if (large == NULL) {
            return 1;
        }
        memset(large, 'x', 1024u * 1024u);
        started = color_tty_now();
        result = img2sixel_color_tty_write(tty, large, 1024u * 1024u,
                                          15, 0, &written);
        free(large);
        if (result != IMG2SIXEL_COLOR_IO_TIMEOUT || written == 0u ||
            written >= 1024u * 1024u ||
            color_tty_now() - started > 0.25) {
            return color_tty_error("write was not bounded and partial");
        }
        /* A slow PTY can reach the deadline before filling its queue.
         * Finish filling it before checking a blocked request/recovery.
         * The parent never drains until the child reports completion. */
        output_copy = open(ttyname(STDOUT_FILENO),
                           O_WRONLY | O_NOCTTY | O_NONBLOCK);
        if (output_copy < 0 || output_copy >= FD_SETSIZE) {
            return color_tty_error("cannot open output filler");
        }
        fill_ready = 1;
        for (written = 0u; written < 1024u * 1024u; ++written) {
            immediate.tv_sec = 0;
            immediate.tv_usec = 0;
            FD_ZERO(&writable);
            FD_SET(output_copy, &writable);
            fill_ready = select(output_copy + 1, NULL, &writable,
                                NULL, &immediate);
            if (fill_ready <= 0 || write(output_copy, "x", 1u) != 1) {
                break;
            }
        }
        close(output_copy);
        if (fill_ready != 0) {
            return color_tty_error("could not fill output queue");
        }
        if (img2sixel_color_tty_close(tty) != IMG2SIXEL_COLOR_IO_OK ||
            img2sixel_color_tty_open(&tty, STDOUT_FILENO, NULL, NULL) !=
                IMG2SIXEL_COLOR_IO_OK ||
            img2sixel_color_tty_begin(tty) != IMG2SIXEL_COLOR_IO_OK) {
            return 1;
        }
        result = img2sixel_color_tty_query(tty, IMG2SIXEL_COLOR_CAPABILITY,
                                          15, &exchange);
        if (result != IMG2SIXEL_COLOR_IO_TIMEOUT ||
            exchange.request_written >= sizeof(COLOR_TTY_QUERY) - 1u ||
            exchange.received) {
            return color_tty_error("blocked request reported silent reply");
        }
        result = img2sixel_color_tty_write(tty, COLOR_TTY_RECOVER,
            sizeof(COLOR_TTY_RECOVER) - 1u, 15, 1, &written);
        if (result != IMG2SIXEL_COLOR_IO_TIMEOUT ||
            written >= sizeof(COLOR_TTY_RECOVER) - 1u) {
            return color_tty_error("blocked restore reported success");
        }
        goto finish;
    }
    started = color_tty_now();
    result = img2sixel_color_tty_query(tty,
        strcmp(name, "policy-failure") == 0 ? IMG2SIXEL_COLOR_POLICY
                                            : IMG2SIXEL_COLOR_CAPABILITY,
        COLOR_TTY_REPLY_TIMEOUT_MS, &exchange);
    if (color_tty_now() - started >
        COLOR_TTY_REPLY_TIMEOUT_MS / 1000.0 + 0.25) {
        return color_tty_error("reply exceeded absolute deadline");
    }
    if (strcmp(name, "silent") == 0 || strcmp(name, "late") == 0) {
        if (result != IMG2SIXEL_COLOR_IO_SILENT || exchange.received ||
            exchange.request_written != sizeof(COLOR_TTY_QUERY) - 1u) {
            return color_tty_error("silent reply classification");
        }
    } else if (strcmp(name, "partial") == 0 || strcmp(name, "flood") == 0) {
        if (result != IMG2SIXEL_COLOR_IO_PARTIAL || !exchange.received ||
            !img2sixel_color_incomplete(&exchange.parser)) {
            return color_tty_error("partial reply classification");
        }
        if (strcmp(name, "flood") == 0 &&
            exchange.parser.payload_size <= sizeof(exchange.parser.header)) {
            return color_tty_error("flood did not exercise payload draining");
        }
    } else if (strcmp(name, "sub-cancel") == 0) {
        if (result != IMG2SIXEL_COLOR_IO_PARTIAL || !exchange.received ||
            img2sixel_color_incomplete(&exchange.parser)) {
            return color_tty_error("SUB was lost before parser cancellation");
        }
    } else if (strcmp(name, "sigint") == 0 || strcmp(name, "sigterm") == 0) {
        if (result != IMG2SIXEL_COLOR_IO_CANCELLED ||
            !color_tty_cancelled) {
            return color_tty_error("cancellation was swallowed");
        }
    } else if (strcmp(name, "invalid") == 0) {
        if (result != IMG2SIXEL_COLOR_IO_OK ||
            exchange.parser.result != IMG2SIXEL_COLOR_INVALID) {
            return color_tty_error("invalid reply classification");
        }
    } else if (strcmp(name, "policy-failure") == 0) {
        if (result != IMG2SIXEL_COLOR_IO_OK ||
            exchange.parser.result != IMG2SIXEL_COLOR_POLICY_FAILURE) {
            return color_tty_error("policy failure classification");
        }
    } else {
        if (result != IMG2SIXEL_COLOR_IO_OK ||
            exchange.parser.result != IMG2SIXEL_COLOR_AVAILABLE ||
            exchange.parser.definition != 4ul ||
            exchange.request_written != sizeof(COLOR_TTY_QUERY) - 1u) {
            return color_tty_error("capability exchange");
        }
        if (strcmp(name, "read-ahead") == 0) {
            if (read(STDOUT_FILENO, &byte, 1u) != 1 || byte != 'z') {
                return color_tty_error("read crossed first ST");
            }
        }
        if (img2sixel_color_tty_end(tty) != IMG2SIXEL_COLOR_IO_OK ||
            !color_tty_restored(&original, &original_mask, flags, 1)) {
            return color_tty_error("phase did not release terminal state");
        }
        if (strcmp(name, "roundtrip") == 0) {
            if (img2sixel_color_tty_begin(tty) != IMG2SIXEL_COLOR_IO_OK ||
                img2sixel_color_tty_query(tty, IMG2SIXEL_COLOR_POLICY,
                    COLOR_TTY_REPLY_TIMEOUT_MS, &exchange) !=
                    IMG2SIXEL_COLOR_IO_OK ||
                exchange.parser.result != IMG2SIXEL_COLOR_SAVED_POLICY ||
                exchange.parser.policy != 0ul) {
                return color_tty_error("second phase policy query");
            }
        }
        if (strcmp(name, "pending-input") == 0) {
            if (!color_tty_expect(STDOUT_FILENO, "pending-after-reply\n")) {
                return color_tty_error("cleanup discarded unread input");
            }
            /* The test's explicit read processes PENDIN; production does
             * not read or flush after returning the terminal to its owner. */
            mode_changed = 0;
        }
        goto finish;
    }
    if (strcmp(name, "late") == 0 &&
        (!color_tty_send(report, "l", 1u) ||
         !color_tty_expect(command, "l"))) {
        return 1;
    }
    /* A delayed reply must not create permission to query again. The only
     * output allowed after failure is the caller's bounded restore write. */
    if (img2sixel_color_tty_query(tty, IMG2SIXEL_COLOR_POLICY,
                                 20, &exchange) != IMG2SIXEL_COLOR_IO_ERROR ||
        exchange.request_written != 0u) {
        return color_tty_error("query retried after correlation loss");
    }
    if (img2sixel_color_tty_write(tty, COLOR_TTY_RECOVER,
        sizeof(COLOR_TTY_RECOVER) - 1u, 20, 1, &written) !=
            IMG2SIXEL_COLOR_IO_OK ||
        written != sizeof(COLOR_TTY_RECOVER) - 1u) {
        return color_tty_error("bounded write-only recovery");
    }
finish:
    if (img2sixel_color_tty_close(tty) != IMG2SIXEL_COLOR_IO_OK ||
        !color_tty_restored(&original, &original_mask, flags, mode_changed)) {
        return color_tty_error("close did not restore terminal state");
    }
    return 0;
}

static int
color_tty_script(char const *name, int master, int command, int report,
                  pid_t child)
{
    char bytes[1024];
    fd_set reads;
    struct timeval timeout;
    double deadline;
    ssize_t count;
    int result;
    int done;

    if (strcmp(name, "backpressure") == 0) {
        if (!color_tty_expect(report, "d")) {
            return 0;
        }
        /* Drain before waiting for child exit: BSD close can wait for its
         * output queue even when the transport descriptor was nonblocking. */
        while ((count = read(master, bytes, sizeof(bytes))) > 0) {
            for (result = 0; result < count; ++result) {
                if (bytes[result] != 'x') {
                    return 0;
                }
            }
        }
        return count == -1 && (errno == EAGAIN || errno == EWOULDBLOCK);
    }
    if (strcmp(name, "non-tty") == 0 || strcmp(name, "unrelated") == 0 ||
        strcmp(name, "identity") == 0 || strcmp(name, "foreground") == 0 ||
        strcmp(name, "inactive") == 0 || strcmp(name, "close-active") == 0) {
        return color_tty_expect(report, "d");
    }
    if (strcmp(name, "stale") == 0) {
        return color_tty_expect(report, "s") &&
            color_tty_send(master, COLOR_TTY_REPLY,
                            sizeof(COLOR_TTY_REPLY) - 1u) &&
            color_tty_send(command, "s", 1u) && color_tty_expect(report, "d");
    }
    if (!color_tty_expect(master, strcmp(name, "policy-failure") == 0
                          ? COLOR_TTY_POLICY : COLOR_TTY_QUERY)) {
        color_tty_error("request bytes");
        return 0;
    }
    if (strcmp(name, "partial") == 0) {
        if (!color_tty_send(master, "\033P1;1", 5u)) {
            return 0;
        }
    } else if (strcmp(name, "sub-cancel") == 0) {
        if (!color_tty_send(master, "\033P1;1\032", 6u)) {
            return 0;
        }
    } else if (strcmp(name, "flood") == 0) {
        if (!color_tty_send(master, "\033P1;1;1;0;2./r", 14u)) {
            return 0;
        }
        memset(bytes, 'x', sizeof(bytes));
        deadline = color_tty_now() + 2.0;
        done = 0;
        while (!done && color_tty_now() < deadline) {
            FD_ZERO(&reads);
            FD_SET(master, &reads);
            /* Keep input arriving across the query deadline, without
             * overrunning the PTY queue and injecting kernel BEL bytes.
             * An inactivity-based timeout would still keep restarting. */
            timeout.tv_sec = 0;
            timeout.tv_usec = 1000;
            result = select(master + 1, &reads, NULL, NULL, &timeout);
            if (result < 0 && errno != EINTR) {
                return 0;
            }
            if (result > 0 && FD_ISSET(master, &reads)) {
                done = color_tty_expect(master, COLOR_TTY_RECOVER);
                if (!done) {
                    return 0;
                }
            } else if (result == 0) {
                count = write(master, bytes, 1u);
                if (count < 0 && errno != EAGAIN && errno != EINTR) {
                    return 0;
                }
            }
        }
        return done && color_tty_expect(report, "d");
    } else if (strcmp(name, "sigint") == 0 || strcmp(name, "sigterm") == 0) {
        if (kill(child, strcmp(name, "sigint") == 0 ? SIGINT : SIGTERM) != 0) {
            return 0;
        }
    } else if (strcmp(name, "invalid") == 0) {
        if (!color_tty_send(master, "\033P1;2;1;1;4./r\033\\", 16u)) {
            return 0;
        }
    } else if (strcmp(name, "policy-failure") == 0) {
        if (!color_tty_send(master, "\033P0$r\033\\", 7u)) {
            return 0;
        }
    } else if (strcmp(name, "late") == 0) {
        if (!color_tty_expect(report, "l") ||
            !color_tty_send(master, "\033P1$r1;3./p\033\\", 13u) ||
            !color_tty_send(command, "l", 1u)) {
            return 0;
        }
    } else if (strcmp(name, "silent") != 0) {
        if (strcmp(name, "eintr") == 0 && kill(child, SIGUSR2) != 0) {
            return 0;
        }
        if (strcmp(name, "sub-recovery") == 0) {
            if (!color_tty_send(master, "\033P1;1\032" COLOR_TTY_REPLY,
                sizeof("\033P1;1\032" COLOR_TTY_REPLY) - 1u)) {
                return 0;
            }
        } else if (strcmp(name, "pending-input") == 0) {
            if (!color_tty_send(master,
                COLOR_TTY_REPLY "pending-after-reply\n",
                sizeof(COLOR_TTY_REPLY "pending-after-reply\n") - 1u)) {
                return 0;
            }
        } else {
            if (!color_tty_send(master, COLOR_TTY_REPLY,
                                sizeof(COLOR_TTY_REPLY) - 1u)) {
                return 0;
            }
        }
        if (strcmp(name, "read-ahead") == 0 &&
            !color_tty_send(master, "z", 1u)) {
            return 0;
        }
        if (strcmp(name, "roundtrip") == 0) {
            if (!color_tty_expect(master, COLOR_TTY_POLICY) ||
                !color_tty_send(master, "\2201$r1;0./p\234", 11u)) {
                return 0;
            }
        }
        return color_tty_expect(report, "d");
    }
    return color_tty_expect(master, COLOR_TTY_RECOVER) &&
        color_tty_expect(report, "d");
}
#endif /* HAVE_CLI_COLOR_PTY && HAVE_CLI_COLOR_TTY */

int
test_cli_0038_color_tty(int argc, char **argv)
{
#if HAVE_CLI_COLOR_PTY && HAVE_CLI_COLOR_TTY
    int master;
    int slave;
    int command[2];
    int report[2];
    int result;
    int status;
    int extra;
    pid_t child;
    char const *path;
    char byte;
    struct termios mode;

    if (argc != 2) {
        return color_tty_error("one script name is required");
    }
    master = posix_openpt(O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (master < 0 || grantpt(master) != 0 || unlockpt(master) != 0) {
        return color_tty_error("cannot allocate PTY");
    }
    path = ptsname(master);
    slave = path == NULL ? -1 : open(path, O_RDWR | O_NOCTTY);
    if (slave < 0 || tcgetattr(slave, &mode) != 0 ||
        pipe(command) != 0 || pipe(report) != 0) {
        close(master);
        return color_tty_error("cannot prepare PTY");
    }
    mode.c_lflag |= ICANON | ECHO;
    mode.c_oflag |= OPOST | ONLCR;
    if (strcmp(argv[1], "pending-input") == 0 ||
        strcmp(argv[1], "stale") == 0 || strcmp(argv[1], "late") == 0 ||
        strcmp(argv[1], "flood") == 0) {
        /* Hurd immediately re-echoes queued raw input when ICANON returns;
         * Darwin waits for another read and Linux does not re-echo it.
         * Observe unread input without that line-discipline side effect.
         * The other scripts still check restoration with ECHO enabled. */
        mode.c_lflag &= ~ECHO;
    }
    if (strcmp(argv[1], "noncanonical") == 0) {
        mode.c_lflag &= ~ICANON;
        mode.c_cc[VMIN] = 3;
        mode.c_cc[VTIME] = 7;
    }
    if (tcsetattr(slave, TCSANOW, &mode) != 0) {
        return 1;
    }
    child = fork();
    if (child == 0) {
        close(master);
        close(command[1]);
        close(report[0]);
        if (setsid() < 0 ||
            (strcmp(argv[1], "unrelated") != 0 &&
             (ioctl(slave, TIOCSCTTY, 0) != 0 ||
              tcsetpgrp(slave, getpgrp()) != 0)) ||
            dup2(slave, STDOUT_FILENO) < 0) {
            _exit(1);
        }
        close(slave);
        result = color_tty_child(argv[1], command[0], report[1]);
        if (!color_tty_send(report[1], "d", 1u)) {
            result = 1;
        }
        _exit(result);
    }
    close(command[0]);
    close(report[1]);
    if (child < 0) {
        return color_tty_error("fork failed");
    }
    result = color_tty_script(argv[1], master, command[1], report[0], child);
    if (!result) {
        kill(child, SIGKILL);
        /* Releasing the master also unblocks BSD's last-slave close when
         * a deliberately broken writer leaves output queued at death. */
        close(master);
        master = -1;
    }
    if (waitpid(child, &status, 0) != child || !WIFEXITED(status) ||
        WEXITSTATUS(status) != 0) {
        result = 0;
    }
    /* The script must explain every outgoing byte. Backpressure deliberately
     * leaves queued filler, and flood can leave echoed input after cleanup. */
    extra = master < 0 ? -1 : (int)read(master, &byte, 1u);
    if (extra > 0 && strcmp(argv[1], "backpressure") != 0 &&
        strcmp(argv[1], "flood") != 0) {
        result = 0;
    }
    if (master >= 0) {
        close(master);
    }
    close(slave);
    close(command[1]);
    close(report[0]);
    return result ? 0 : color_tty_error("script failed");
#else
    (void)argc;
    (void)argv;
    return 77;
#endif
}
