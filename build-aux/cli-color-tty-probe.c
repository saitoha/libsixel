/* SPDX-License-Identifier: MIT */
/* Shared compile/link probe; never executed by either build system. */
#ifndef _POSIX_C_SOURCE
# define _POSIX_C_SOURCE 200809L
#endif
#ifndef _XOPEN_SOURCE
# define _XOPEN_SOURCE 700
#endif
#if defined(_WIN32) || defined(__EMSCRIPTEN__)
# error CLI terminal color I/O requires a POSIX terminal
#endif
#include <fcntl.h>
#include <signal.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>
#ifdef IMG2SIXEL_COLOR_PTY_PROBE
# include <sys/ioctl.h>
# include <sys/wait.h>
#endif

int
main(void)
{
    int fd;
    int result;
    char byte;
    struct timespec now;
    struct timeval timeout;
    struct stat identity;
    struct termios mode;
    sigset_t mask;
    fd_set ready;

    result = clock_gettime(CLOCK_MONOTONIC, &now);
    fd = open(ttyname(1), O_RDWR | O_NOCTTY | O_NONBLOCK);
    result += fcntl(fd, F_SETFD, FD_CLOEXEC);
    result += fstat(fd, &identity) + isatty(fd);
    result += tcgetpgrp(fd) == getpgrp();
    result += tcgetattr(fd, &mode);
    mode.c_iflag &= ~(ISTRIP | INLCR | IGNCR | ICRNL | IXON | IXOFF | PARMRK);
    mode.c_lflag &= ~(ICANON | ECHO | ECHONL | IEXTEN);
    mode.c_cc[VSUSP] = _POSIX_VDISABLE;
    mode.c_cc[VMIN] = 1;
    mode.c_cc[VTIME] = 0;
    result += tcsetattr(fd, TCSANOW, &mode);
    result += sigemptyset(&mask);
    result += sigaddset(&mask, SIGTTIN) + sigaddset(&mask, SIGTTOU);
    result += sigprocmask(SIG_BLOCK, &mask, NULL);
    result += sigprocmask(SIG_SETMASK, &mask, NULL);
    FD_ZERO(&ready);
    FD_SET(0, &ready);
    timeout.tv_sec = 0;
    timeout.tv_usec = 0;
    result += select(1, &ready, NULL, NULL, &timeout);
    result += (int)read(fd, &byte, 1u);
    result += (int)write(fd, &byte, 1u);
    result += close(fd);
#ifdef IMG2SIXEL_COLOR_PTY_PROBE
    fd = posix_openpt(O_RDWR | O_NOCTTY | O_NONBLOCK);
    result += grantpt(fd) + unlockpt(fd);
    result += ptsname(fd) != NULL;
    result += fork() < 0;
    result += setsid() < 0;
    result += ioctl(fd, TIOCSCTTY, 0);
    result += tcsetpgrp(fd, getpgrp());
    result += waitpid(-1, NULL, WNOHANG) < 0;
#endif
    return result;
}
