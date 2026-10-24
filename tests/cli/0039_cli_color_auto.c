/* SPDX-License-Identifier: MIT */
/* Literal PTY peer for the actual img2sixel executable, not a mock encoder. */
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
#if HAVE_CLI_COLOR_PTY && HAVE_CLI_COLOR_TTY
# include <errno.h>
# include <fcntl.h>
# include <signal.h>
# include <sys/ioctl.h>
# include <sys/select.h>
# include <sys/stat.h>
# include <sys/types.h>
# include <sys/wait.h>
# include <termios.h>
# include <time.h>
# include <unistd.h>

#define COLOR_AUTO_CAP "\033[?1./r"
#define COLOR_AUTO_QUERY "\033P$q1./p\033\\"
#define COLOR_AUTO_P3 "\033P1;1;1;0;4./r\033\\"
#define COLOR_AUTO_SRGB "\033P1;1;1;0;3./r\033\\"
#define COLOR_AUTO_CAN "\030\033\\"

typedef struct color_auto_peer {
    int master;
    int slave;
    int data;
    int errors;
    int file_fd;
    int drcs;
    int cursor_restore;
    char file_path[96];
    pid_t child;
    double deadline;
    char trace[32768];
    size_t trace_size;
    char image[32768];
    char first_image[4096];
    size_t image_size;
} color_auto_peer_t;

int test_cli_0036_color_wire(int argc, char **argv);

static double
color_auto_now(void)
{
    struct timespec now;

    clock_gettime(CLOCK_MONOTONIC, &now);
    return (double)now.tv_sec + (double)now.tv_nsec / 1000000000.0;
}

static int
color_auto_error(char const *message)
{
    fprintf(stderr, "CLI color auto: %s (errno=%d)\n", message, errno);
    return 0;
}

/* Drain diagnostic output concurrently: actual CMS traces and dyld output
 * must not fill a pipe and accidentally stand in for protocol timing. */
static int
color_auto_byte(color_auto_peer_t *peer, char *byte)
{
    fd_set ready;
    struct timeval wait;
    int maximum;
    int result;
    ssize_t count;
    double left;

    for (;;) {
        left = peer->deadline - color_auto_now();
        if (left <= 0.0) {
            return color_auto_error("script deadline");
        }
        wait.tv_sec = (time_t)left;
        wait.tv_usec = (long)((left - wait.tv_sec) * 1000000.0);
        FD_ZERO(&ready);
        FD_SET(peer->data, &ready);
        maximum = peer->data;
        if (peer->errors >= 0) {
            FD_SET(peer->errors, &ready);
            if (maximum < peer->errors) {
                maximum = peer->errors;
            }
        }
        result = select(maximum + 1, &ready, NULL, NULL, &wait);
        if (result < 0 && errno == EINTR) {
            continue;
        }
        if (result <= 0) {
            return color_auto_error("script readiness");
        }
        if (peer->errors >= 0 && FD_ISSET(peer->errors, &ready)) {
            count = read(peer->errors, peer->trace + peer->trace_size,
                         sizeof(peer->trace) - peer->trace_size - 1u);
            if (count > 0) {
                peer->trace_size += (size_t)count;
                peer->trace[peer->trace_size] = '\0';
                if (peer->trace_size == sizeof(peer->trace) - 1u) {
                    return color_auto_error("diagnostic bound exceeded");
                }
            } else if (count == 0) {
                close(peer->errors);
                peer->errors = -1;
            }
        }
        if (FD_ISSET(peer->data, &ready)) {
            count = read(peer->data, byte, 1u);
            if (count == 1) {
                return 1;
            }
            if (count == 0 || (errno != EAGAIN && errno != EINTR)) {
                return 0;
            }
        }
    }
}

static int
color_auto_expect(color_auto_peer_t *peer, char const *text)
{
    char byte;

    while (*text != '\0') {
        if (!color_auto_byte(peer, &byte) || byte != *text++) {
            return color_auto_error("unexpected or missing control byte");
        }
    }
    return 1;
}

static int
color_auto_reply(color_auto_peer_t *peer, char const *text)
{
    size_t size;

    size = strlen(text);
    return write(peer->master, text, size) == (ssize_t)size;
}

static int
color_auto_policy_reply(color_auto_peer_t *peer, int policy)
{
    char text[] = "\033P1$r1;0./p\033\\";

    text[7] = (char)('0' + policy);
    return color_auto_reply(peer, text);
}

static int
color_auto_policy_expect(color_auto_peer_t *peer, int policy)
{
    char text[] = "\033[1;0./p";

    text[4] = (char)('0' + policy);
    return color_auto_expect(peer, text);
}

static int
color_auto_frame(color_auto_peer_t *peer)
{
    char byte;
    char previous;

    peer->image_size = 0u;
    previous = '\0';
    while (color_auto_byte(peer, &byte)) {
        if (peer->image_size + 1u >= sizeof(peer->image)) {
            return color_auto_error("image bound exceeded");
        }
        peer->image[peer->image_size++] = byte;
        peer->image[peer->image_size] = '\0';
        if (previous == '\033' && byte == '\\') {
            /* The legacy no-termios library restores the cursor even for
             * static output. Accept only this exact existing trailer at
             * the encoder boundary, leaving all later bytes to the strict
             * tail check and all color controls to the transaction script. */
            if (peer->cursor_restore &&
                !color_auto_expect(peer, "\033[?25h")) {
                return 0;
            }
            return peer->image_size > 8u && peer->image[0] == '\033' &&
                (peer->drcs ? strstr(peer->image, "{ 0") != NULL
                            : peer->image[1] == 'P');
        }
        previous = byte;
    }
    return color_auto_error("missing SIXEL frame");
}

static int
color_auto_is(char const *name, char const *expected)
{
    return strcmp(name, expected) == 0;
}

static int
color_auto_no_query(char const *name, int baseline)
{
    return baseline || strncmp(name, "drcs-", 5u) == 0 ||
        color_auto_is(name, "stdout-pipe") ||
        color_auto_is(name, "stdout-file") ||
        color_auto_is(name, "outfile") ||
        color_auto_is(name, "outfile-file-dash") ||
        color_auto_is(name, "explicit-gamma") ||
        color_auto_is(name, "explicit-p3-file") ||
        color_auto_is(name, "cli-over-env") ||
        color_auto_is(name, "env-gamma") ||
        color_auto_is(name, "env-empty") ||
        color_auto_is(name, "env-assignment-overridden") ||
        color_auto_is(name, "timeout-min") ||
        strncmp(name, "bad-", 4u) == 0;
}

static void
color_auto_child(char const *name, char const *binary, char const *fixture,
                 int baseline, int slave, int errors, int output,
                 char *file_path)
{
    char *arguments[48];
    int count;
    int input;
    int status;
    pid_t worker;
    struct termios original;
    struct termios current;
    tcflag_t expected_flags;
    char const *color_option;
    int input_pipe[2];
    ssize_t input_size;
    char input_bytes[4096];

    alarm(3);
    if (setsid() < 0 || ioctl(slave, TIOCSCTTY, 0) < 0 ||
        tcsetpgrp(slave, getpgrp()) < 0 || dup2(slave, STDOUT_FILENO) < 0 ||
        dup2(errors, STDERR_FILENO) < 0) {
        _exit(120);
    }
    close(slave);
    close(errors);
    /* Keep the session leader alive until after the real CLI exits. Darwin
     * revokes the slave when its session leader exits, so checking termios
     * only from the outer parent would observe ENOTTY instead of cleanup. */
    if (tcgetattr(STDOUT_FILENO, &original) != 0) {
        _exit(123);
    }
    signal(SIGINT, SIG_IGN);
    signal(SIGTERM, SIG_IGN);
    worker = fork();
    if (worker < 0) {
        _exit(124);
    }
    if (worker > 0) {
        if (waitpid(worker, &status, 0) != worker || !WIFEXITED(status)) {
            _exit(125);
        }
        expected_flags = original.c_lflag;
#if defined(__APPLE__) && defined(__MACH__) && defined(PENDIN)
        if (!color_auto_no_query(name, baseline)) {
            expected_flags |= PENDIN;
        }
#endif
        if (tcgetattr(STDOUT_FILENO, &current) != 0 ||
            current.c_iflag != original.c_iflag ||
            current.c_oflag != original.c_oflag ||
            current.c_cflag != original.c_cflag ||
            current.c_lflag != expected_flags ||
            memcmp(current.c_cc, original.c_cc, sizeof(current.c_cc)) != 0 ||
            cfgetispeed(&current) != cfgetispeed(&original) ||
            cfgetospeed(&current) != cfgetospeed(&original)) {
            fprintf(stderr, "CLI color auto: terminal settings changed\n");
            _exit(126);
        }
        _exit(WEXITSTATUS(status));
    }
    signal(SIGINT, SIG_DFL);
    signal(SIGTERM, SIG_DFL);
    alarm(3);
    unsetenv("SIXEL_OUTPUT_COLORSPACE");
    unsetenv("SIXEL_OUTPUT_COLORSPACE_AUTO_TIMEOUT_MS");
    setenv("SIXEL_LOADER_OSC11_BG_QUERY", "0", 1);
    if (!baseline && color_auto_is(name, "encoder-reader")) {
        setenv("SIXEL_LOADER_OSC11_BG_QUERY", "1", 1);
    }
    setenv("SIXEL_TRACE_TOPIC", "loader", 1);
    if (!baseline && (color_auto_is(name, "stdout-pipe") ||
        color_auto_is(name, "stdout-file"))) {
        /* stderr is deliberately a valid terminal while stdout is not. */
        if (dup2(STDOUT_FILENO, STDERR_FILENO) < 0 ||
            dup2(output, STDOUT_FILENO) < 0) {
            _exit(127);
        }
        setenv("SIXEL_TRACE_TOPIC", "", 1);
    } else if (!baseline && color_auto_is(name, "explicit-p3-file")) {
        if (dup2(output, STDOUT_FILENO) < 0) {
            _exit(127);
        }
    }
    if (output >= 0 && output != STDOUT_FILENO) {
        close(output);
    }
    count = 0;
    arguments[count++] = (char *)binary;
    arguments[count++] = "--threading-policy=greedy:global_budget=1";
    arguments[count++] = "-7";
    arguments[count++] = "-z0";
    arguments[count++] =
        "-Llibpng:cms_engine=builtin:cms_target=linear:prefer_8bit=0!";
    arguments[count++] = "-jscalar:resize_precision=float";
    arguments[count++] = "--precision-policy=auto";
    arguments[count++] = "-Qkmeans:seed=1";
    arguments[count++] = "-Xlinear";
    arguments[count++] = "-Wlinear";
    arguments[count++] = "-Fnone";
    arguments[count++] = "-aoff";
    arguments[count++] = "-_none";
    arguments[count++] = "-~none";
    arguments[count++] = "-dnone";
    arguments[count++] = "-trgb";
    arguments[count++] = "--binning-policy=none";
    arguments[count++] = "--sampling-policy=full-frame";
    if (!baseline && color_auto_is(name, "encoder-reader")) {
        /* The input remains opaque RGB. Composite policy activates the
         * existing OSC11 reader without introducing alpha-bearing input. */
        arguments[count++] = "-Acomposite";
    }
    if (strncmp(name, "drcs-", 5u) == 0) {
        arguments[count++] = "-@2:1";
    }
    /* Semantic cases use the normal 200 ms exchange budget. A short budget
     * also times the peer's scheduling under parallel check, not just the
     * protocol. Immediate replies do not wait for the budget to expire.
     * Partial/save/set/restore timeouts must first reach their intended
     * phase, so their preceding peer replies also keep the normal budget.
     * Silent probes and explicit duration checks can use shorter waits. */
    color_option = "-Uauto";
    if (color_auto_is(name, "silent") ||
        color_auto_is(name, "multiple-silent")) {
        color_option = "-Uauto:timeout_ms=20";
    } else if (color_auto_is(name, "timeout-long")) {
        color_option = "-Uauto:timeout_ms=40";
    } else if (color_auto_is(name, "timeout-compact")) {
        color_option = "-Uauto:T40";
    } else if (color_auto_is(name, "timeout-min")) {
        color_option = "-Uauto:T1";
    } else if (color_auto_is(name, "timeout-max")) {
        color_option = "-Uauto:T10000";
    } else if (color_auto_is(name, "timeout-env") ||
               color_auto_is(name, "bad-env-timeout")) {
        color_option = "-Uauto";
        setenv("SIXEL_OUTPUT_COLORSPACE_AUTO_TIMEOUT_MS",
               color_auto_is(name, "timeout-env") ? "20" : "0", 1);
    } else if (color_auto_is(name, "timeout-cli-over-env")) {
        color_option = "-Uauto:timeout_ms=20";
        setenv("SIXEL_OUTPUT_COLORSPACE_AUTO_TIMEOUT_MS", "invalid", 1);
    } else if (color_auto_is(name, "ray")) {
        color_option = "-Uauto:gamut=ray-trace:T200";
    } else if (color_auto_is(name, "edge")) {
        color_option = "-Uauto:gamut=edge-seeker:T200";
    } else if (color_auto_is(name, "gamut")) {
        color_option = "-Uauto:gamut=local-minde:T200";
    } else if (color_auto_is(name, "prefix")) {
        color_option = "-UAu:T200";
    } else if (color_auto_is(name, "bad-zero")) {
        color_option = "-Uauto:T0";
    } else if (color_auto_is(name, "bad-limit")) {
        color_option = "-Uauto:T10001";
    } else if (color_auto_is(name, "bad-signed")) {
        color_option = "-Uauto:T-1";
    } else if (color_auto_is(name, "bad-overflow")) {
        color_option = "-Uauto:T999999999999999999999999999";
    } else if (color_auto_is(name, "bad-empty")) {
        color_option = "-Uauto:timeout_ms=";
    } else if (color_auto_is(name, "bad-key-prefix")) {
        color_option = "-Uauto:timeout=20";
    }
    if (!baseline && (color_auto_is(name, "env-assignment") ||
        color_auto_is(name, "drcs-assignment"))) {
        arguments[count++] = "--env";
        arguments[count++] = "SIXEL_OUTPUT_COLORSPACE=auto";
        arguments[count++] = "--env";
        arguments[count++] = "SIXEL_OUTPUT_COLORSPACE_AUTO_TIMEOUT_MS=200";
    } else if (!baseline && (color_auto_is(name, "env-auto") ||
        color_auto_is(name, "drcs-env") ||
        color_auto_is(name, "env-gamma") ||
        color_auto_is(name, "env-empty") ||
        color_auto_is(name, "bad-env-base"))) {
        setenv("SIXEL_OUTPUT_COLORSPACE", (color_auto_is(name, "env-auto") ||
            color_auto_is(name, "drcs-env")) ? "auto"
            : color_auto_is(name, "env-gamma") ? "gamma"
            : color_auto_is(name, "env-empty") ? "" : "invalid", 1);
        setenv("SIXEL_OUTPUT_COLORSPACE_AUTO_TIMEOUT_MS", "200", 1);
    } else {
        arguments[count++] = baseline
            ? (color_auto_is(name, "gamut")
                ? "-Ugamma:gamut=local-minde"
                : color_auto_is(name, "edge")
                    ? "-Ugamma:gamut=edge-seeker"
                    : color_auto_is(name, "ray")
                        ? "-Ugamma:gamut=ray-trace" : "-Ugamma")
            : (char *)color_option;
    }
    if (!baseline && color_auto_is(name, "drcs-gamma")) {
        arguments[count - 1] = "-Ugamma";
    }
    if (!baseline && color_auto_is(name, "drcs-p3")) {
        arguments[count - 1] = "-Udisplay-p3";
    }
    if (!baseline && color_auto_is(name, "drcs-overridden")) {
        arguments[count++] = "-Ugamma";
    }
    if (!baseline && color_auto_is(name, "base-invalid-ignored")) {
        setenv("SIXEL_OUTPUT_COLORSPACE", "invalid", 1);
    }
    if (!baseline && (color_auto_is(name, "explicit-gamma") ||
        color_auto_is(name, "cli-over-env"))) {
        setenv("SIXEL_OUTPUT_COLORSPACE", "auto", 1);
        arguments[count++] = "-Ugamma";
    }
    if (!baseline && color_auto_is(name, "repeated-auto")) {
        arguments[count++] = "-Ugamma";
        arguments[count++] = "-Uauto:T200";
    }
    if (!baseline && color_auto_is(name, "env-assignment-overridden")) {
        arguments[count++] = "-Ugamma";
        arguments[count++] = "--env";
        arguments[count++] = "SIXEL_OUTPUT_COLORSPACE=auto";
    }
    if (!baseline && color_auto_is(name, "explicit-p3-file")) {
        arguments[count++] = "-Udisplay-p3";
    }
    if (!baseline && (color_auto_is(name, "outfile") ||
        color_auto_is(name, "outfile-file-dash") ||
        color_auto_is(name, "timeout-min"))) {
        arguments[count++] = "-o";
        arguments[count++] = file_path;
    }
    if (!baseline && (color_auto_is(name, "outfile-dash") ||
        color_auto_is(name, "outfile-file-dash"))) {
        arguments[count++] = "-o-";
    }
    if (!baseline && color_auto_is(name, "encode-error")) {
        /* K-means feedback remains outside the P3 path and fails after
         * policy negotiation, exercising restoration after encode failure. */
        arguments[count++] = "-Qkmeans:feedback=1";
    }
    if (!baseline && color_auto_is(name, "stdin")) {
        input = open(fixture, O_RDONLY);
        input_size = input < 0 ? -1 : read(input, input_bytes,
                                           sizeof(input_bytes));
        if (input < 0 || input_size <= 0 ||
            input_size == (ssize_t)sizeof(input_bytes) ||
            pipe(input_pipe) != 0 ||
            write(input_pipe[1], input_bytes, (size_t)input_size) !=
                input_size || dup2(input_pipe[0], STDIN_FILENO) < 0) {
            _exit(121);
        }
        close(input);
        close(input_pipe[0]);
        close(input_pipe[1]);
    } else {
        arguments[count++] = (char *)fixture;
        if (!baseline && (color_auto_is(name, "multiple") ||
            color_auto_is(name, "multiple-silent"))) {
            arguments[count++] = (char *)fixture;
        }
    }
    arguments[count] = NULL;
    execv(binary, arguments);
    _exit(122);
}

static int
color_auto_start(color_auto_peer_t *peer, char const *name,
                 char const *binary, char const *fixture, int baseline)
{
    int errors[2];
    int output[2];
    int to_file;
    char const *path;
    struct winsize geometry;

    memset(peer, 0, sizeof(*peer));
    peer->file_fd = -1;
    peer->drcs = strncmp(name, "drcs-", 5u) == 0;
    output[0] = -1;
    output[1] = -1;
    to_file = !baseline && (color_auto_is(name, "stdout-file") ||
        color_auto_is(name, "outfile") ||
        color_auto_is(name, "outfile-file-dash") ||
        color_auto_is(name, "explicit-p3-file") ||
        color_auto_is(name, "timeout-min"));
#if !(HAVE_TERMIOS_H && HAVE_SYS_IOCTL_H && HAVE_ISATTY && \
      HAVE_TCGETATTR && HAVE_TCSETATTR)
    peer->cursor_restore = !to_file &&
        (baseline || !color_auto_is(name, "stdout-pipe"));
#endif
    if (to_file) {
        strcpy(peer->file_path, "/tmp/libsixel-color-auto-XXXXXX");
        peer->file_fd = mkstemp(peer->file_path);
        if (peer->file_fd < 0) {
            return color_auto_error("cannot create inspected output file");
        }
    }
    if (!baseline && color_auto_is(name, "stdout-pipe") &&
        pipe(output) != 0) {
        return 0;
    }
    peer->master = posix_openpt(O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (peer->master < 0 || grantpt(peer->master) < 0 ||
        unlockpt(peer->master) < 0) {
        return color_auto_error("cannot allocate PTY");
    }
    path = ptsname(peer->master);
    peer->slave = path == NULL ? -1 : open(path, O_RDWR | O_NOCTTY);
    if (peer->slave < 0 || pipe(errors) < 0) {
        return color_auto_error("cannot prepare peer");
    }
    if (peer->drcs) {
        memset(&geometry, 0, sizeof(geometry));
        geometry.ws_col = 80;
        geometry.ws_row = 24;
        geometry.ws_xpixel = 640;
        geometry.ws_ypixel = 384;
        if (ioctl(peer->slave, TIOCSWINSZ, &geometry) != 0) {
            return color_auto_error("cannot provide DRCS cell geometry");
        }
    }
    peer->child = fork();
    if (peer->child == 0) {
        close(peer->master);
        close(errors[0]);
        if (output[0] >= 0) {
            close(output[0]);
        }
        color_auto_child(name, binary, fixture, baseline,
                          peer->slave, errors[1],
                          to_file ? peer->file_fd : output[1],
                          peer->file_path);
    }
    close(errors[1]);
    peer->errors = errors[0];
    peer->data = output[0] >= 0 ? output[0] : peer->master;
    if (peer->data != peer->master &&
        fcntl(peer->data, F_SETFL, O_NONBLOCK) < 0) {
        return 0;
    }
    if (output[1] >= 0) {
        close(output[1]);
    }
    peer->deadline = color_auto_now() + 3.0;
    return peer->child > 0;
}

static int
color_auto_empty(int fd)
{
    char bytes[256];
    ssize_t count;

    do {
        count = read(fd, bytes, sizeof(bytes));
    } while (count < 0 && errno == EINTR);
    if (count > 0) {
        return color_auto_error("unexpected trailing output");
    }
    return count == 0 || errno == EAGAIN || errno == EWOULDBLOCK ||
        errno == EIO;
}

static int
color_auto_finish(color_auto_peer_t *peer, int valid, int expected_exit)
{
    int status;
    int count;
    fd_set ready;
    struct timeval wait;

    if (valid) {
        /* The parent's open slave prevents PTY EOF. Wait for either process
         * exit or unexpected bytes, without sleeps or an unbounded waitpid. */
        for (;;) {
            if (!color_auto_empty(peer->master) ||
                (peer->data != peer->master &&
                 !color_auto_empty(peer->data))) {
                valid = 0;
                break;
            }
            count = (int)waitpid(peer->child, &status, WNOHANG);
            if (count == peer->child) {
                break;
            }
            if (count < 0 || color_auto_now() >= peer->deadline) {
                valid = color_auto_error("child did not exit");
                break;
            }
            FD_ZERO(&ready);
            FD_SET(peer->master, &ready);
            wait.tv_sec = 0;
            wait.tv_usec = 1000;
            select(peer->master + 1, &ready, NULL, NULL, &wait);
        }
        /* A writer can enqueue its last bytes immediately before exit.
         * Reaping it is not evidence that either output queue is empty. */
        if (valid && (!color_auto_empty(peer->master) ||
            (peer->data != peer->master && !color_auto_empty(peer->data)))) {
            valid = 0;
        }
    }
    if (!valid) {
        kill(-peer->child, SIGKILL);
        if (peer->data == peer->master) {
            peer->data = -1;
        }
        close(peer->master);
        peer->master = -1;
        waitpid(peer->child, &status, 0);
    }
    if (!WIFEXITED(status) || WEXITSTATUS(status) != expected_exit) {
        valid = color_auto_error("unexpected child exit status");
    }
    if (peer->errors >= 0) {
        while ((count = (int)read(peer->errors,
                peer->trace + peer->trace_size,
                sizeof(peer->trace) - peer->trace_size - 1u)) > 0) {
            peer->trace_size += (size_t)count;
        }
        peer->trace[peer->trace_size] = '\0';
        close(peer->errors);
    }
    if (peer->file_fd >= 0) {
        if (lseek(peer->file_fd, 0, SEEK_SET) < 0) {
            valid = 0;
        }
        count = (int)read(peer->file_fd, peer->image,
                          sizeof(peer->image) - 1u);
        if (count <= 0 || count == (int)sizeof(peer->image) - 1) {
            valid = color_auto_error("empty or oversized file output");
        } else {
            peer->image_size = (size_t)count;
            peer->image[count] = '\0';
            if (memchr(peer->image, '\0', (size_t)count) != NULL ||
                strstr(peer->image, "\033\\") != peer->image + count - 2) {
                valid = color_auto_error("file contains non-image bytes");
            }
        }
        close(peer->file_fd);
        unlink(peer->file_path);
    }
    if (peer->data >= 0 && peer->data != peer->master) {
        close(peer->data);
    }
    if (peer->master >= 0) {
        close(peer->master);
    }
    close(peer->slave);
    if (!valid) {
        fprintf(stderr, "%s\n", peer->trace);
    }
    return valid;
}

static int
color_auto_script(color_auto_peer_t *peer, char const *name,
                  int *expected_exit, int *p3)
{
    int saved;
    int selected;
    int fail_save;
    int fail_set;
    int fail_restore;
    int encode_error;
    int iteration;
    int images;
    char const *capability;
    double started;
    double elapsed;
    double minimum;

    saved = strncmp(name, "save-", 5u) == 0 ? name[5] - '0' : 0;
    selected = color_auto_is(name, "srgb") ||
        color_auto_is(name, "gamut") || color_auto_is(name, "edge") ||
        color_auto_is(name, "ray") ||
        color_auto_is(name, "unsupported") ||
        color_auto_is(name, "simple") || color_auto_is(name, "icc") ||
        color_auto_is(name, "p3-no-s2") ? 1 : 2;
    fail_save = color_auto_is(name, "save-failure") ||
        color_auto_is(name, "save-timeout");
    fail_set = color_auto_is(name, "set-mismatch") ||
        color_auto_is(name, "set-timeout");
    fail_restore = color_auto_is(name, "restore-mismatch") ||
        color_auto_is(name, "restore-timeout");
    encode_error = color_auto_is(name, "encode-error");
    images = color_auto_is(name, "multiple") ||
        color_auto_is(name, "multiple-silent") ? 2 : 1;
    *expected_exit = 0;
    *p3 = selected == 2;
    if (strncmp(name, "bad-", 4u) == 0 ||
        color_auto_is(name, "drcs-auto") ||
        color_auto_is(name, "drcs-env") ||
        color_auto_is(name, "drcs-assignment")) {
        *expected_exit = 2;
        return 1;
    }
    if (color_auto_no_query(name, 0)) {
        *p3 = color_auto_is(name, "explicit-p3-file") ||
            color_auto_is(name, "drcs-p3");
        return peer->file_fd >= 0 || color_auto_frame(peer);
    }
    if (!color_auto_expect(peer, COLOR_AUTO_CAP)) {
        return 0;
    }
    if (color_auto_is(name, "timeout-long") ||
        color_auto_is(name, "timeout-compact") ||
        color_auto_is(name, "timeout-env") ||
        color_auto_is(name, "timeout-cli-over-env")) {
        /* Observe the effective timeout, not merely acceptance of its
         * spelling. Start after the real request, excluding exec latency. */
        *p3 = 0;
        minimum = color_auto_is(name, "timeout-long") ||
            color_auto_is(name, "timeout-compact") ? 0.025 : 0.010;
        started = color_auto_now();
        if (!color_auto_frame(peer)) {
            return 0;
        }
        elapsed = color_auto_now() - started;
        return elapsed >= minimum && elapsed < 0.150;
    }
    if (color_auto_is(name, "silent") ||
        color_auto_is(name, "multiple-silent")) {
        *p3 = 0;
        if (!color_auto_frame(peer)) {
            return 0;
        }
        if (images == 2) {
            if (peer->image_size >= sizeof(peer->first_image)) {
                return 0;
            }
            strcpy(peer->first_image, peer->image);
            return color_auto_frame(peer) &&
                strcmp(peer->first_image, peer->image) == 0;
        }
        return 1;
    }
    if (color_auto_is(name, "legacy")) {
        *p3 = 0;
        return color_auto_reply(peer, "\033P1;0;0;0;3./r\033\\") &&
            color_auto_frame(peer);
    }
    if (color_auto_is(name, "partial") || color_auto_is(name, "invalid")) {
        *expected_exit = 1;
        return color_auto_reply(peer, color_auto_is(name, "partial")
            ? "\033P1;1;" : "\033P1;1;0;0;4./r\033\\");
    }
    if (color_auto_is(name, "sigint") || color_auto_is(name, "sigterm")) {
        *expected_exit = 1;
        return kill(-peer->child, color_auto_is(name, "sigint")
                                  ? SIGINT : SIGTERM) == 0;
    }
    capability = selected == 1 ? COLOR_AUTO_SRGB : COLOR_AUTO_P3;
    if (color_auto_is(name, "unsupported")) {
        capability = "\033P1;1;1;0;5./r\033\\";
    } else if (color_auto_is(name, "simple")) {
        capability = "\033P1;1;1;1;1;0;22;22./r\033\\";
    } else if (color_auto_is(name, "icc")) {
        capability = "\033P1;1;1;0;2./runsupported\033\\";
    } else if (color_auto_is(name, "p3-no-s2")) {
        capability = "\033P1;1;0;1;4./r\033\\";
    }
    if (!color_auto_reply(peer, capability)) {
        return 0;
    }
    for (iteration = 0; iteration < images; ++iteration) {
        if (iteration != 0) {
            saved = 3;
        }
        if (!color_auto_expect(peer, COLOR_AUTO_QUERY)) {
            return 0;
        }
        if (fail_save) {
            *expected_exit = 1;
            return color_auto_is(name, "save-timeout") ||
                color_auto_reply(peer, "\033P0$r1./p\033\\");
        }
        if (!color_auto_policy_reply(peer, saved) ||
            !color_auto_policy_expect(peer, selected) ||
            !color_auto_expect(peer, COLOR_AUTO_QUERY)) {
            return 0;
        }
        if (fail_set) {
            *expected_exit = 1;
            return (color_auto_is(name, "set-timeout") ||
                    color_auto_policy_reply(peer, 3)) &&
                color_auto_policy_expect(peer, saved);
        }
        if (color_auto_is(name, "set-sigint") ||
            color_auto_is(name, "set-sigterm")) {
            *expected_exit = 1;
            return kill(-peer->child, color_auto_is(name, "set-sigint")
                                          ? SIGINT : SIGTERM) == 0 &&
                color_auto_policy_expect(peer, saved);
        }
        if (!color_auto_policy_reply(peer, selected)) {
            return 0;
        }
        if (color_auto_is(name, "encoder-reader") &&
            (!color_auto_expect(peer, "\033]11;?\007") ||
             !color_auto_reply(peer, "\033]11;rgb:0000/0000/0000\007"))) {
            return 0;
        }
        if (encode_error) {
            *expected_exit = 2;
            if (peer->cursor_restore &&
                !color_auto_expect(peer, "\033[?25h")) {
                return 0;
            }
            if (!color_auto_expect(peer, COLOR_AUTO_CAN)) {
                return 0;
            }
        } else if (!color_auto_frame(peer)) {
            return 0;
        }
        if (images == 2) {
            if (iteration == 0) {
                if (peer->image_size >= sizeof(peer->first_image)) {
                    return 0;
                }
                strcpy(peer->first_image, peer->image);
            } else if (strcmp(peer->first_image, peer->image) != 0) {
                return color_auto_error("second image differs");
            }
        }
        if (!color_auto_policy_expect(peer, saved) ||
            !color_auto_expect(peer, COLOR_AUTO_QUERY)) {
            return 0;
        }
        if (fail_restore) {
            *expected_exit = 1;
            return color_auto_is(name, "restore-timeout") ||
                color_auto_policy_reply(peer, 3);
        }
        if (color_auto_is(name, "restore-sigint")) {
            *expected_exit = 1;
            return kill(-peer->child, SIGINT) == 0;
        }
        if (!color_auto_policy_reply(peer, saved)) {
            return 0;
        }
    }
    return 1;
}
#endif

int
test_cli_0039_color_auto(int argc, char **argv)
{
#if HAVE_CLI_COLOR_PTY && HAVE_CLI_COLOR_TTY
    color_auto_peer_t peer;
    color_auto_peer_t baseline;
    /* Include both complete buffers and the synthetic SIXEL introducer. */
    char combined[sizeof(peer.trace) + sizeof(peer.image) + 3];
    char *wire_arguments[3];
    int valid;
    int expected_exit;
    int p3;
    int combined_size;
    char *next_trace;
    char const *drcs_prefix;
    size_t case_index;
    static char const *const cases[] = {
        "save-0",
        "save-1",
        "save-2",
        "save-3",
        "srgb",
        "legacy",
        "silent",
        "unsupported",
        "partial",
        "invalid",
        "save-failure",
        "save-timeout",
        "set-mismatch",
        "set-timeout",
        "restore-mismatch",
        "restore-timeout",
        "sigint",
        "sigterm",
        "encode-error",
        "stdin",
        "stdout-pipe",
        "stdout-file",
        "outfile",
        "outfile-dash",
        "outfile-file-dash",
        "explicit-gamma",
        "explicit-p3-file",
        "timeout-long",
        "timeout-compact",
        "timeout-min",
        "timeout-max",
        "timeout-env",
        "timeout-cli-over-env",
        "prefix",
        "gamut", "edge", "ray",
        "bad-zero",
        "bad-limit",
        "bad-signed",
        "bad-overflow",
        "bad-empty",
        "bad-key-prefix",
        "bad-env-timeout",
        "env-auto",
        "env-gamma",
        "env-empty",
        "bad-env-base",
        "base-invalid-ignored",
        "cli-over-env",
        "repeated-auto",
        "simple",
        "icc",
        "p3-no-s2",
        "multiple",
        "set-sigint",
        "set-sigterm",
        "restore-sigint",
        "env-assignment",
        "env-assignment-overridden",
        "multiple-silent",
        "encoder-reader",
        "drcs-auto",
        "drcs-env",
        "drcs-assignment",
        "drcs-overridden",
        "drcs-gamma",
        "drcs-p3",
    };

    if (argc != 4) {
        return 1;
    }
#if !(HAVE_TCGETATTR && HAVE_TCSETATTR)
    if (color_auto_is(argv[1], "encoder-reader")) {
        return 77;
    }
#endif
    for (case_index = 0u; case_index < sizeof(cases) / sizeof(cases[0]);
         ++case_index) {
        if (strcmp(argv[1], cases[case_index]) == 0) {
            break;
        }
    }
    if (case_index == sizeof(cases) / sizeof(cases[0])) {
        return 1;
    }
    if (!color_auto_start(&peer, argv[1], argv[2], argv[3], 0)) {
        return 1;
    }
    valid = color_auto_script(&peer, argv[1], &expected_exit, &p3);
    if (!color_auto_finish(&peer, valid, expected_exit)) {
        return 1;
    }
    if (expected_exit != 0) {
        return peer.image_size != 0u &&
            !color_auto_is(argv[1], "restore-mismatch") &&
            !color_auto_is(argv[1], "restore-timeout") &&
            !color_auto_is(argv[1], "restore-sigint");
    }
    if (p3) {
        if (color_auto_is(argv[1], "multiple")) {
            next_trace = strstr(peer.trace, "libsixel: path probe begin:");
            if (next_trace == NULL || (next_trace = strstr(next_trace + 1,
                    "libsixel: path probe begin:")) == NULL) {
                return 1;
            }
            *next_trace = '\0';
        }
        if (peer.drcs) {
            /* Check the glyph envelope from this isolated PTY's geometry,
             * then feed only its SIXEL body to the independent RGB oracle.
             * No color query or policy write may precede the definition. */
            drcs_prefix = "\033[?8800h\033P1;0;0;8;1;3;16;0{ 0";
            if (strncmp(peer.image, drcs_prefix, strlen(drcs_prefix)) != 0) {
                return color_auto_error("unexpected DRCS envelope");
            }
            combined_size = snprintf(combined, sizeof(combined),
                "%s\033Pq%.*s", peer.trace, (int)sizeof(peer.image) - 1,
                peer.image + strlen(drcs_prefix));
        } else {
            combined_size = snprintf(combined, sizeof(combined), "%s%s",
                peer.trace, peer.image);
        }
        if (combined_size < 0 || (size_t)combined_size >= sizeof(combined)) {
            return color_auto_error("RGB oracle input exceeds buffer");
        }
        wire_arguments[0] = "cli/0036_cli_color_wire";
        wire_arguments[1] = "libpng-p3-16";
        wire_arguments[2] = combined;
        return test_cli_0036_color_wire(3, wire_arguments);
    }
    if (!color_auto_start(&baseline, argv[1], argv[2], argv[3], 1)) {
        return 1;
    }
    valid = color_auto_frame(&baseline);
    if (!color_auto_finish(&baseline, valid, 0)) {
        return 1;
    }
    return peer.image_size != baseline.image_size ||
        memcmp(peer.image, baseline.image, peer.image_size) != 0;
#else
    (void)argc;
    (void)argv;
    return 77;
#endif
}
