/* SPDX-License-Identifier: MIT */
/* Send a real terminal interrupt after img2sixel consumes an input line. */
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
#if HAVE_CLI_COLOR_PTY
# include <errno.h>
# include <fcntl.h>
# include <signal.h>
# include <sys/ioctl.h>
# include <sys/select.h>
# include <sys/wait.h>
# include <termios.h>
# include <time.h>
# include <unistd.h>
#endif

#if HAVE_CLI_COLOR_PTY && defined(FIONREAD)
static double
stdin_interrupt_now(void)
{
    struct timespec now;

    clock_gettime(CLOCK_MONOTONIC, &now);
    return (double)now.tv_sec + (double)now.tv_nsec / 1e9;
}

int
test_cli_stdin_interrupt(int argc, char **argv)
{
    int master;
    int slave;
    int null_fd;
    int output[2];
    int flags;
    int pending;
    int status;
    int result;
    pid_t child;
    pid_t worker;
    pid_t waited;
    char *path;
    char byte;
    struct termios original;
    struct termios current;
    struct timeval timeout;
    fd_set ready;
    double deadline;

    master = -1;
    slave = -1;
    null_fd = -1;
    output[0] = -1;
    output[1] = -1;
    child = -1;
    result = 1;
    if (argc != 2) {
        return 1;
    }
    master = posix_openpt(O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (master < 0 || grantpt(master) < 0 || unlockpt(master) < 0) {
        goto end;
    }
    path = ptsname(master);
    slave = path == NULL ? -1 : open(path, O_RDWR | O_NOCTTY);
    null_fd = open("/dev/null", O_WRONLY);
    if (slave < 0 || null_fd < 0 || pipe(output) < 0 ||
        tcgetattr(slave, &original) < 0) {
        goto end;
    }
    original.c_lflag |= ICANON | ISIG;
    original.c_lflag &= (tcflag_t)~(ECHO | ECHONL);
    original.c_cc[VINTR] = 3;
    if (tcsetattr(slave, TCSANOW, &original) < 0 ||
        (flags = fcntl(slave, F_GETFL)) < 0) {
        goto end;
    }
    /* Queue Enter before exec and observe its readiness. Once FIONREAD
     * reaches zero, the converter has consumed it, even when fread is
     * still waiting to fill a 4096-byte request. No timing guess is needed. */
    if (write(master, "\n", 1u) != 1) {
        goto end;
    }
    FD_ZERO(&ready);
    FD_SET(slave, &ready);
    timeout.tv_sec = 2;
    timeout.tv_usec = 0;
    if (select(slave + 1, &ready, NULL, NULL, &timeout) != 1 ||
        ioctl(slave, FIONREAD, &pending) < 0 || pending != 1) {
        goto end;
    }
    child = fork();
    if (child == 0) {
        close(master);
        close(output[0]);
        if (setsid() < 0 || ioctl(slave, TIOCSCTTY, 0) < 0 ||
            tcsetpgrp(slave, getpgrp()) < 0 ||
            dup2(slave, STDIN_FILENO) < 0 ||
            dup2(null_fd, STDOUT_FILENO) < 0 ||
            dup2(null_fd, STDERR_FILENO) < 0) {
            _exit(120);
        }
        close(slave);
        close(null_fd);
        /* Keep the session leader alive like an interactive shell. macOS
         * revokes the slave when its leader exits, preventing inspection
         * of the converter's restored terminal settings afterward. */
        signal(SIGINT, SIG_IGN);
        worker = fork();
        if (worker == 0) {
            close(output[1]);
            signal(SIGINT, SIG_DFL);
            execl(argv[1], argv[1], (char *)NULL);
            _exit(121);
        }
        if (worker < 0) {
            _exit(122);
        }
        do {
            waited = waitpid(worker, &status, 0);
        } while (waited < 0 && errno == EINTR);
        if (waited != worker ||
            write(output[1], &status, sizeof(status)) != sizeof(status)) {
            _exit(123);
        }
        for (;;) {
            pause();
        }
    }
    close(output[1]);
    output[1] = -1;
    if (child < 0) {
        goto end;
    }
    deadline = stdin_interrupt_now() + 3.0;
    while (pending != 0) {
        if (ioctl(slave, FIONREAD, &pending) < 0 ||
            stdin_interrupt_now() >= deadline) {
            fprintf(stderr, "stdin interrupt: input was not consumed\n");
            goto end;
        }
        timeout.tv_sec = 0;
        timeout.tv_usec = 1000;
        (void)select(0, NULL, NULL, NULL, &timeout);
    }
    /* VINTR goes through the line discipline to the foreground process.
     * Keep both PTY ends open: EOF must not rescue a stuck input reader. */
    byte = 3;
    if (write(master, &byte, 1u) != 1) {
        goto end;
    }
    FD_ZERO(&ready);
    FD_SET(output[0], &ready);
    timeout.tv_sec = 2;
    timeout.tv_usec = 0;
    if (select(output[0] + 1, &ready, NULL, NULL, &timeout) != 1 ||
        read(output[0], &status, sizeof(status)) != sizeof(status)) {
        fprintf(stderr, "stdin interrupt: Ctrl-C did not stop input\n");
        goto end;
    }
    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0 ||
        fcntl(slave, F_GETFL) != flags ||
        tcgetattr(slave, &current) < 0 ||
        current.c_iflag != original.c_iflag ||
        current.c_oflag != original.c_oflag ||
        current.c_cflag != original.c_cflag ||
        current.c_lflag != original.c_lflag ||
        memcmp(current.c_cc, original.c_cc, sizeof(original.c_cc)) != 0) {
        fprintf(stderr,
                "stdin interrupt: exit or terminal state mismatch "
                "(status=%d flags=%d/%d)\n", status,
                fcntl(slave, F_GETFL), flags);
        goto end;
    }
    result = 0;

end:
    if (child > 0) {
        kill(-child, SIGKILL);
        kill(child, SIGKILL);
        /* Closing the master also releases a terminal drain during exit. */
        close(master);
        master = -1;
        while (waitpid(child, NULL, 0) < 0 && errno == EINTR) {
        }
    }
    if (output[0] >= 0) {
        close(output[0]);
    }
    if (output[1] >= 0) {
        close(output[1]);
    }
    if (null_fd >= 0) {
        close(null_fd);
    }
    if (slave >= 0) {
        close(slave);
    }
    if (master >= 0) {
        close(master);
    }
    return result;
}
#else
int
test_cli_stdin_interrupt(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return 77;
}
#endif
