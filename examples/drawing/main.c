
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <sixel.h>
#include <termios.h>
#include <errno.h>
#include <string.h>
#include <sys/select.h>

static int sixel_write(char *data, int size, void *priv)
{
    return fwrite(data, 1, size, (FILE *)priv);
}

int main(int argc, char **argv)
{
    int width = 640;
    int height = 480;
    unsigned char *buf;
    SIXELSTATUS status = SIXEL_FALSE;
    sixel_output_t *output = NULL;
    sixel_dither_t *dither = NULL;
    struct termios old_termios;
    struct termios new_termios;
    int x1 = 1000;
    int y1 = 1000;
    int x0 = (-1);
    int y0 = (-1);
    fd_set rfds;
    struct timeval tv;
    unsigned char ibuf[256];
    unsigned char *p;
    int n;
    unsigned int params[1024];
    int param = 0;
    int params_idx = 0;
    int ibytes;
    int state;

    printf("\033[?8452h" "\033[?1070l" "\033[1;1'z");

    buf = calloc(1, width * height);

    tcgetattr(STDIN_FILENO, &old_termios);
    memcpy(&new_termios, &old_termios, sizeof(old_termios));
    new_termios.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    new_termios.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
    new_termios.c_cflag &= ~(CSIZE | PARENB);
    new_termios.c_cflag |= CS8;
    new_termios.c_oflag &= ~(OPOST);
    new_termios.c_cc[VMIN] = 1;
    new_termios.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &new_termios);
    printf("\033[?25l\033[H");
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

    status = sixel_output_new(&output, sixel_write, stdout, NULL);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    dither = sixel_dither_get(SIXEL_BUILTIN_G8);
    sixel_dither_set_pixelformat(dither, SIXEL_PIXELFORMAT_G8);
    status = sixel_encode(buf, width, height, 0, dither, output);
    if (SIXEL_FAILED(status)) {
        goto end;
    }

    for (state = STATE_GROUND;;) {

        printf("\033['|");
       	fflush(stdout);
        tv.tv_sec = 0;
        tv.tv_usec = 10000;
        FD_ZERO(&rfds);
        FD_SET(STDIN_FILENO, &rfds);
        if (select(STDIN_FILENO + 1, &rfds, NULL, NULL, &tv) < 0)
            goto end;
        n = read(STDIN_FILENO, ibuf, sizeof(ibuf));
        for (p = ibuf; p < ibuf + n; ++p) {
            switch (state) {
            case STATE_GROUND:
                switch (*p) {
                case 0x03:  /*  */
                    tv.tv_sec = 0;
                    tv.tv_usec = 10000;
                    FD_ZERO(&rfds);
                    FD_SET(STDIN_FILENO, &rfds);
                    if (select(STDIN_FILENO + 1, &rfds, NULL, NULL, &tv) > 0)
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
                case 0x3b:           /* separator, : to ; */
                    params[params_idx++] = param;
                    param = 0;
                    break;
                case 0x3c ... 0x3f:  /* parameter, < to ? */
                    ibytes = ibytes << 8 | *p;
                    break;
                case 0x40 ... 0x4f:  /* Final byte, @ to ~ */
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
                        y1 = params[2];			     
                        x1 = params[3];			     
                        if ((x0 != x1 || y0 != y1) && x1 < width && y1 < height) {
                            if (x0 > 0 && y0 > 0 && params[1] == 4) {
                                int dx = x0 > x1 ? x0 - x1 : x1 - x0;
                                int dy = y0 > y1 ? y0 - y1 : y1 - y0;
                                int sx = x0 < x1 ? 1 : -1;
                                int sy = y0 < y1 ? 1 : -1;
                                int err = dx - dy;
                                int e2;
                                
                                while (1) {
                                    buf[width * y0 + x0] = 0xff;
                                    if (x0 == x1 && y0 == y1)
                                        break;
                                    e2 = 2 * err;
                                    if (e2 > - dy) {
                                        err -= dy;
                                        x0 += sx;
                                    }
                                    if (e2 < dx) {
                                        err += dx;
                                        y0 += sy;
                                    }
                                }
                                printf("\033[H");
                                status = sixel_encode(buf, width, height, 0, dither, output);
                                if (SIXEL_FAILED(status)) {
                                    goto end;
                                }

                            }
                            x0 = x1;
                            y0 = y1;
                        } 
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
    printf("\033[?25h\033c");
    fflush(stdout);
    tcsetattr(STDIN_FILENO, TCSADRAIN, &old_termios);

    free(buf);

    printf("\n");

    return 0;
}

/* EOF */
