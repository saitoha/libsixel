/**
 * An example program demonstrating the integration of DEC Locator Mode and SGR-Pixels
 *
 * Hayaki Saito <saitoha@me.com>
 *
 * I declared this program is in Public Domain (CC0 - "No Rights Reserved"),
 * This file is offered AS-IS, without any warranty.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <termios.h>
#include <string.h>
#include <sys/select.h>
#include <sys/ioctl.h>
#include <sixel.h>

struct canvas {
    int width;
    int height;
    unsigned char *buf;
    int x0;
    int y0;
    int x1;
    int y1;
};

static int
canvas_init(struct canvas *c, int width, int height)
{
    c->width = width;
    c->height = height;
    c->x1 = (-1);
    c->y1 = (-1);
    c->x0 = (-1);
    c->y0 = (-1);
    c->buf = calloc(1, width * height);

    return 0;
}


static void
canvas_deinit(struct canvas *c)
{
    free(c->buf);
}


static void
canvas_point(struct canvas *c, int x, int y)
{
    c->x1 = x;
    c->y1 = y;
}


static void
canvas_sync(struct canvas *c)
{
    c->x0 = c->x1;
    c->y0 = c->y1;
}

static int
canvas_redraw(struct canvas *c, sixel_dither_t *dither, sixel_output_t *output) {
    printf("\0338");
    if (SIXEL_FAILED(sixel_encode(c->buf, c->width, c->height, 0, dither, output)))
        return 1;
    return 0;
}

static int
wait_stdin(int usec)
{
    fd_set rfds;
    struct timeval tv;

    tv.tv_sec = usec / 1000000;
    tv.tv_usec = usec % 1000000;
    FD_ZERO(&rfds);
    FD_SET(STDIN_FILENO, &rfds);
    return select(STDIN_FILENO + 1, &rfds, NULL, NULL, &tv);
}


static int
scroll_on_demand(int pixelheight)
{
    struct winsize size = {0, 0, 0, 0};
    int row = 0;
    int col = 0;
    int cellheight;
    int scroll;

    ioctl(STDOUT_FILENO, TIOCGWINSZ, &size);
    if (size.ws_ypixel <= 0)
        goto err;
    /* request cursor position report */
    printf("\033[6n");
    if (wait_stdin(1000 * 1000) <= 0)  /* wait 1 sec */
        goto err;
    if (scanf("\033[%d;%dR", &row, &col) != 2)
        goto err;
    cellheight = pixelheight * size.ws_row / size.ws_ypixel + 1;
    scroll = cellheight + row - size.ws_row;
    printf("\033[%dS\033[%dA", scroll, scroll);
    printf("\0337");

    return size.ws_ypixel / size.ws_row * (row - (scroll < 0 ? 0: scroll) - 1);

err:
    printf("\033[H\0337");
    return 0;
}

/* set terminal into raw mode. */
static int
tty_raw(struct termios *old_termios)
{
    struct termios new_termios;
    int ret;

    ret = tcgetattr(STDIN_FILENO, old_termios);
    if (ret < 0)
        return ret;
    memcpy(&new_termios, old_termios, sizeof(*old_termios));
    new_termios.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    new_termios.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
    new_termios.c_cflag &= ~(CSIZE | PARENB);
    new_termios.c_cflag |= CS8;
    new_termios.c_oflag &= ~(OPOST);
    new_termios.c_cc[VMIN] = 1;
    new_termios.c_cc[VTIME] = 0;
    ret = tcsetattr(STDIN_FILENO, TCSAFLUSH, &new_termios);
    if (ret < 0)
        return ret;

    return 0;
}

static void tty_restore(struct termios *old_termios)
{
    (void) tcsetattr(STDIN_FILENO, TCSADRAIN, old_termios);
}

static int sixel_write(char *data, int size, void *priv)
{
    return fwrite(data, 1, size, (FILE *)priv);
}

static int draw_line(struct canvas const *c)
{
    int dx;
    int dy;
    int sx;
    int sy;
    int err;
    int err2;
    int x0;
    int y0;
    int x1;
    int y1;

    if (c->x0 < 0 || c->y0 < 0 || c->x0 >= c->width || c->y0 >= c->height)
        return 1;

    x0 = c->x0;
    y0 = c->y0;
    x1 = c->x1;
    y1 = c->y1;

    if (x1 < 0)
        x1 = 0;
    if (y1 < 0)
        y1 = 0;
    if (x1 >= c->width)
        x1 = c->width - 1;
    if (y1 >= c->height)
        y1 = c->height - 1;
    if (x0 == x1 && y0 == y1)
        return 1;

    dx = x0 > x1 ? x0 - x1 : x1 - x0;
    dy = y0 > y1 ? y0 - y1 : y1 - y0;
    sx = x0 < x1 ? 1 : -1;
    sy = y0 < y1 ? 1 : -1;
    err = dx - dy;

    for (;;) {
        c->buf[c->width * y0 + x0] = 0xff;
        if (x0 == x1 && y0 == y1)
            break;
        err2 = 2 * err;
        if (err2 > - dy) {
            err -= dy;
            x0 += sx;
        }
        if (err2 < dx) {
            err += dx;
            y0 += sy;
        }
    }

    return 0;
}

enum {
    MODE_AUTO = 0,
    MODE_SGR  = 1,
    MODE_DEC  = 2
};

enum {
    STATE_GROUND            = 0,
    STATE_ESC               = 1,
    STATE_ESC_INTERMEDIATE  = 2,
    STATE_CSI_PARAMETER     = 3,
    STATE_CSI_INTERMEDIATE  = 4,
    STATE_SS                = 5,
    STATE_OSC               = 6,
    STATE_STR               = 7
};

int has_sixel_capability()
{
    unsigned char ibuf[256];
    unsigned char *p;
    int param = 0;
    int n;
    int has_4 = 0;

    printf("\033[c");  /* DA1 query */
    fflush(stdout);

    if (wait_stdin(10000) <= 0)
        goto failed;
    if ((n = read(STDIN_FILENO, ibuf, sizeof(ibuf))) < 0)
        goto failed;
    if (n >= sizeof(ibuf))
        goto failed;
    ibuf[n] = '\0';
    if (n < 4)
        goto failed;

    if (*(p = ibuf) != '\033' || *++p != '[' || *++p != '?')
        goto failed;

    while (*++p) {
        switch (*p) {
            case 0x30 ... 0x39:  /* parameter, 0 to 9 */
                param = param * 10 + *p - 0x30;
                break;
            case 0x3b:           /* separator, ; */
                if (param == 4)
                    has_4 = 1;
                param = 0;
                break;
            case 0x63:           /* DA response final byte "c" */
                if (param == 4)
                    has_4 = 1;
                if (has_4)
                    goto success;
            case 0x3a:           /* separator, : */
            default:
                /* invalid response */
                goto failed;
        }
    }

failed:
    return 0;

success:
    return 1;
}

int has_sgr_pixels()
{
    unsigned char ibuf[256];
    int n;

    printf("\033[?1016$p");
    fflush(stdout);

    if (wait_stdin(10000) <= 0)
        goto failed;
    if ((n = read(STDIN_FILENO, ibuf, sizeof(ibuf))) < 0)
        goto failed;
    if (n >= sizeof(ibuf))
        goto failed;

    ibuf[n] = '\0';
    if (strcmp((const char *)ibuf, "\033[?1016;2$y") != 0)
        if (strcmp((const char *)ibuf, "\033[?1016;1$y") != 0)
            goto failed;

    return 1;

failed:
    return 0;
}


int has_dec_locator()
{
    unsigned char ibuf[256];
    int n;

    printf("\033[?55n");
    fflush(stdout);

    if (wait_stdin(10000) < 0)
        goto failed;
    if ((n = read(STDIN_FILENO, ibuf, sizeof(ibuf))) < 0)
        goto failed;
    if (n >= sizeof(ibuf))
        goto failed;

    ibuf[n] = '\0';
    if (strcmp((const char *)ibuf, "\033[?50n") != 0)
        goto failed;

    return 1;

failed:
    return 0;
}


int main(int argc, char **argv)
{
    SIXELSTATUS status = SIXEL_FALSE;
    struct canvas c = {0, 0, NULL, 0, 0, 0, 0};
    sixel_output_t *output = NULL;
    sixel_dither_t *dither = NULL;
    struct termios old_termios;
    unsigned char ibuf[256];
    unsigned char *p;
    int n;
    unsigned int params[1024];
    int param = 0;
    int params_idx = 0;
    int ibytes;
    int state;
    int offset;
    int mode = 0;
    int reqmode = MODE_AUTO;

    if (argc > 1) {
        if (strcmp(argv[1], "sgr") == 0)
            reqmode = MODE_SGR;
        else if (strcmp(argv[1], "dec") == 0)
            reqmode = MODE_DEC;
    }

    (void) tty_raw(&old_termios);

    if (! has_sixel_capability())
        goto end;

    if (reqmode != MODE_DEC && has_sgr_pixels()) {
        printf("\033[?1000;1003;1016h");
        mode = MODE_SGR;
        printf("\033]2;SGR-pixels reporting mode\007");
    } else if (reqmode != MODE_SGR && has_dec_locator()) {
        printf("\033[1;1'z");
        mode = MODE_DEC;
        printf("\033]2;DEC locator mode\007");
    } else {
        goto end;
    }

    printf("\033[?25l" "\0338");
    printf("\033[?1;3;256S" "\033[?80;8452h");

    if (canvas_init(&c, 640, 480) != 0)
        return (-1);

    offset = scroll_on_demand(c.height);

    status = sixel_output_new(&output, sixel_write, stdout, NULL);
    if (SIXEL_FAILED(status))
        goto end;

    dither = sixel_dither_get(SIXEL_BUILTIN_G8);
    sixel_dither_set_pixelformat(dither, SIXEL_PIXELFORMAT_G8);
    status = sixel_encode(c.buf, c.width, c.height, 0, dither, output);
    if (SIXEL_FAILED(status))
        goto end;

    /* initial drawing */
    canvas_redraw(&c, dither, output);

    for (state = STATE_GROUND;;) {
        if (mode == MODE_DEC) {
            printf("\033['|");
            fflush(stdout);
            if (wait_stdin(10000) < 0)
                goto end;
        }
        if ((n = read(STDIN_FILENO, ibuf, sizeof(ibuf))) < 0)
            goto end;
        for (p = ibuf; p < ibuf + n; ++p) {
            switch (state) {
            case STATE_GROUND:
                switch (*p) {
                case 0x03:  /*  */
                    if (wait_stdin(10000) >= 0)
                        n = read(STDIN_FILENO, ibuf, sizeof(ibuf));
                    goto end;
                    break;
                case 0x1b:  /* ESC */
                    state = STATE_ESC;
                    break;
                default:
                    /* ignore */
                    break;
                }
                break;
            case STATE_ESC:
                /*
                 * - ISO-6429 independent escape sequense
                 *
                 *     ESC F
                 *
                 * - ISO-2022 designation sequence
                 *
                 *     ESC I ... I F
                 */
                switch (*p) {
                case 0x5b:  /* [ */
                    ibytes = 0;
                    param = 0;
                    params_idx = 0;
                    state = STATE_CSI_PARAMETER;
                    break;
                case 0x5d:  /* ] */
                    state = STATE_OSC;
                    break;
                case 0x4e ... 0x4f:  /* N / O */
                    state = STATE_SS;
                    break;
                case 0x50: /* P(DCS) */
                case 0x58: /* X(SOS) */
                case 0x5e ... 0x5f: /* ^(PM) / _(APC) */
                    state = STATE_STR;
                    break;
                case 0x1b: /* ESC */
                    state = STATE_ESC;
                    break;
                case 0x18:
                case 0x1a:
                    state = STATE_GROUND;
                    break;
                case 0x00 ... 0x17:
                case 0x19:
                case 0x1c ... 0x1f:
                case 0x7f:
                    /* ignore */
                    break;
                case 0x20 ... 0x2f:  /* SP to / */
                    ibytes = ibytes << 8 | *p;
                    state = STATE_ESC_INTERMEDIATE;
                    break;
                default:
                    state = STATE_GROUND;
                    break;
                }
                break;
            case STATE_CSI_PARAMETER:
                /*
                 * parse control sequence
                 *
                 * CSI P ... P I ... I F
                 *     ^
                 */
                switch (*p) {
                case 0x00 ... 0x17:
                case 0x19:
                case 0x1c ... 0x1f:
                case 0x7f:
                    /* ignore */
                    break;
                case 0x18:  /* CAN */
                case 0x1a:  /* SUB */
                    state = STATE_GROUND;
                    break;
                case 0x1b:  /* ESC */
                    state = STATE_ESC;
                    break;
                case 0x20 ... 0x2f:  /* intermediate, SP to / */
                    params[params_idx++] = param;
                    param = 0;
                    ibytes = ibytes << 8 | *p;
                    state = STATE_CSI_INTERMEDIATE;
                    break;
                case 0x30 ... 0x39:  /* parameter, 0 to 9 */
                    param = param * 10 + *p - 0x30;
                    break;
                case 0x3a:           /* separator, : */
                    ibytes = ibytes << 8;
                    break;
                case 0x3b:           /* separator, ; */
                    params[params_idx++] = param;
                    param = 0;
                    break;
                case 0x3c ... 0x3f:  /* parameter, < to ? */
                    ibytes = ibytes << 8 | *p;
                    break;
                case 0x40 ... 0x4f:  /* Final byte, @ to ~ */
                    params[params_idx++] = param;
                    ibytes = ibytes << 8 | *p;
                    state = STATE_GROUND;
                    if (ibytes == ('<' << 8 | 'M') && params_idx >= 3) {
                        /* printf("\033]2;%d;%d;%d\007", params[0], params[1], params[2]); fflush(stdout); */
                        canvas_point(&c, params[1], params[2] - offset);
                        if (params[0] == 32 && draw_line(&c) == 0)
                            if (canvas_redraw(&c, dither, output) != 0)
                                goto end;
                        canvas_sync(&c);
                    }
                default:
                    state = STATE_GROUND;
                    break;
                }
                break;
            case STATE_CSI_INTERMEDIATE:
                /*
                 * parse control sequence
                 *
                 * CSI P ... P I ... I F
                 *             ^
                 */
                switch (*p) {
                case 0x00 ... 0x17:
                case 0x19:
                case 0x1c ... 0x1f:
                case 0x7f:
                    /* ignore */
                    break;
                case 0x18:
                case 0x1a:
                    state = STATE_GROUND;
                    break;
                case 0x1b:  /* ESC */
                    state = STATE_ESC;
                    break;
                case 0x20 ... 0x2f:  /* intermediate, SP to / */
                    ibytes = ibytes << 8 | *p;
                    break;
                case 0x30 ... 0x3f:
                    state = STATE_GROUND;
                    break;
                case 0x40 ... 0x7e:  /* Final byte, @ to ~ */
                    ibytes = ibytes << 8 | *p;
                    state = STATE_GROUND;
                    if (ibytes == ('&' << 8 | 'w') && params_idx >= 4) {
                        canvas_point(&c, params[3], params[2] - offset);
                        if (params[1] == 4 && draw_line(&c) == 0)
                            if (canvas_redraw(&c, dither, output) != 0)
                                goto end;
                        canvas_sync(&c);
                    }
                    break;
                default:
                    state = STATE_GROUND;
                    break;
                }
                break;
            case STATE_ESC_INTERMEDIATE:
                switch (*p) {
                case 0x00 ... 0x17:
                case 0x19:
                case 0x1c ... 0x1f:
                case 0x7f:
                    /* ignore */
                    break;
                case 0x18:
                case 0x1a:
                    state = STATE_GROUND;
                    break;
                case 0x1b:  /* ESC */
                    state = STATE_ESC;
                    break;
                case 0x20 ... 0x2f:  /* SP to / */
                    state = STATE_ESC_INTERMEDIATE;
                    break;
                case 0x30 ... 0x3f:  /* 0 to ~, Final byte */
                    state = STATE_GROUND;
                    break;
                }
                break;
            case STATE_OSC:
                switch (*p) {
                case 0x07:
                case 0x18:
                case 0x1a:
                    state = STATE_GROUND;
                    break;
                case 0x1b:
                    state = STATE_ESC;
                    break;
                default:
                    break;
                }
                break;
            case STATE_STR:
                switch (*p) {
                case 0x18:
                case 0x1a:
                    state = STATE_GROUND;
                    break;
                case 0x1b:
                    state = STATE_ESC;
                    break;
                default:
                    break;
                }
                break;
            case STATE_SS:
                switch (*p) {
                case 0x1b:
                    state = STATE_ESC;
                    break;
                default:
                    state = STATE_GROUND;
                    break;
                }
                break;
            }
        }
    }

end:
    /* clean up */
    printf("\033[?25h\033]2;\007");
    if (mode == MODE_DEC)
        printf("\033[0;1'z");
    else if (mode == MODE_SGR)
        printf("\033[?1000;1003;1016l");
    fflush(stdout);

    tty_restore(&old_termios);

    canvas_deinit(&c);

    printf("\n");

    return 0;
}

/* vim: set et ts=4 */
/* EOF */
