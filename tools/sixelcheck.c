/*
 * Copyright (c) 2014,2015 Hayaki Saito
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include "config.h"
#include <stdio.h>
#include <stdlib.h>

#if HAVE_TERMIOS_H
# include <termios.h>
#endif
#if HAVE_UNISTD_H
# include <unistd.h>
#endif
#if HAVE_MEMORY_H
# include <memory.h>
#endif
#if HAVE_SYS_SELECT_H
# include <sys/select.h>
#endif
#if HAVE_SIGNAL_H
# include <signal.h>
#endif
#if HAVE_SYS_SIGNAL_H
# include <sys/signal.h>
#endif

#if HAVE_TERMIOS_H && HAVE_SYS_IOCTL_H && HAVE_ISATTY
static int
tty_cbreak(struct termios *old_termios, struct termios *new_termios)
{
    int status = (-1);
    int ret;

    /* set the terminal to cbreak mode */
    ret = tcgetattr(STDIN_FILENO, old_termios);
    if (ret != 0) {
        perror("tty_cbreak: tcgetattr() failed.");
        goto end;
    }

    (void) memcpy(new_termios, old_termios, sizeof(*old_termios));
    new_termios->c_lflag &= ~(ECHO | ICANON);
    new_termios->c_cc[VMIN] = 1;
    new_termios->c_cc[VTIME] = 0;

    ret = tcsetattr(STDIN_FILENO, TCSAFLUSH, new_termios);
    if (ret != 0) {
        perror("tty_cbreak: tcsetattr() failed.");
        goto end;
    }

    status = (0);

end:
    return status;
}
#endif  /* HAVE_TERMIOS_H && HAVE_SYS_IOCTL_H && HAVE_ISATTY */


#if HAVE_TERMIOS_H && HAVE_SYS_IOCTL_H && HAVE_ISATTY
static int
tty_restore(struct termios *old_termios)
{
    int status = (-1);
    int ret;

    ret = tcsetattr(STDIN_FILENO, TCSAFLUSH, old_termios);
    if (ret != 0) {
        perror("tty_restore: tcsetattr() failed.");
        goto end;
    }

    status = (0);

end:
    return status;
}
#endif  /* HAVE_TERMIOS_H && HAVE_SYS_IOCTL_H && HAVE_ISATTY */


static int
wait_stdin(int usec)
{
#if HAVE_SYS_SELECT_H
    fd_set rfds;
    struct timeval tv;
#endif  /* HAVE_SYS_SELECT_H */
    int ret = 0;

#if HAVE_SYS_SELECT_H
    tv.tv_sec = usec / 1000000;
    tv.tv_usec = usec % 1000000;
    FD_ZERO(&rfds);
    FD_SET(STDIN_FILENO, &rfds);
    ret = select(STDIN_FILENO + 1, &rfds, NULL, NULL, &tv);
#else
    (void) usec;
#endif  /* HAVE_SYS_SELECT_H */

    return ret;
}


#if HAVE_SIGNAL

static int signaled = 0;

static void
signal_handler(int sig)
{
    signaled = sig;
}

#endif


int
main(int argc, char *argv[])
{
    int status = EXIT_FAILURE;
    int param = 0;
    int sixel_available = 0;
    int nwrite;
    ssize_t nread;
    struct termios old_termios;
    struct termios new_termios;
    char buf[256];
    char *p;

    (void) argc;
    (void) argv;

    /* set the terminal to cbreak mode */
    status = tty_cbreak(&old_termios, &new_termios);
    if (status != (0)) {
        return status;
    }

    /* set signal handler to handle SIGINT/SIGTERM/SIGHUP */
#if HAVE_SIGNAL
# if HAVE_DECL_SIGINT
    signal(SIGINT, signal_handler);
# endif
# if HAVE_DECL_SIGTERM
    signal(SIGTERM, signal_handler);
# endif
# if HAVE_DECL_SIGHUP
    signal(SIGHUP, signal_handler);
# endif
#endif

    /* request cursor position report */
    nwrite = printf("\033[>c");
    if (nwrite < 0) {
        perror("main: printf() failed.");
        goto end;
    }

    /* wait cursor position report */
    if (wait_stdin(1000 * 1000) == (-1)) { /* wait up to 1 sec */
        nwrite = printf("\033[H");
        if (nwrite < 0) {
            perror("main: printf() failed.");
            goto end;
        }
        goto end;
    }

    nread = read(STDIN_FILENO, buf, sizeof(buf));
    if (nread < 0) {
        perror("main: read() failed.");
        goto end;
    }
    p = (char *)buf;

    if (p++ >= buf + nread) {
        goto end;
    }
    if (*p == EOF || *p != '\033') {
        goto end;
    }

    if (p++ >= buf + nread) {
        goto end;
    }
    if (*p == EOF || *p != '[') {
        goto end;
    }

    while (p++ < buf + nread) {
        if ('0' <= *p && *p <= '9') {
            param = param * 10 + *p - '0';
        } else if (*p == ';') {
            if (param == 4) {
                sixel_available = 1;
            }
            param = 0;
        } else if (*p == 'c') {
            if (param == 4) {
                sixel_available = 1;
            }
            break;
        } else {
            sixel_available = 0;
        }
    }

    if (!sixel_available) {
        status = EXIT_FAILURE;
    }

end:
    /* restore the terminal mode */
    status = tty_restore(&old_termios);
    if (status != (0)) {
        goto end;
    }

    return status;
}

/* emacs, -*- Mode: C; tab-width: 4; indent-tabs-mode: nil -*- */
/* vim: set expandtab ts=4 : */
/* EOF */
