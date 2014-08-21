/*
 * Copyright (c) 2014 kmiya@culti
 * Copyright (c) 2014 Hayaki Saito
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>

static unsigned char *
pnm_get_line(unsigned char *p, unsigned char *e, unsigned char *line)
{
    int n;

    do {
        for (n = 0 ; p < e && *p >= ' '; p++) {
            if (n < 255)
                line[n++] = *p;
        }

        if (p < e && *p == '\n')
            p++;

        line[n] = '\0';

    } while (line[0] == '#');

    return p;
}


unsigned char *
load_pnm(unsigned char *p, int len, int *psx, int *psy, int *pcomp, int *pstride)
{
    int n, i, b, x, y, c[3];
    int ascii, maps;
    int width, height, deps;
    unsigned char *result;
    unsigned char *s, *e, tmp[256];

    width = height = 0;
    deps = 1;

    e = p + len;
    p = pnm_get_line(p, e, tmp);

    if (tmp[0] != 'P')
        return NULL;

    switch(tmp[1]) {
    case '1':
        ascii = 1;
        maps  = 0;
        break;
    case '2':
        ascii = 1;
        maps  = 1;
        break;
    case '3':
        ascii = 1;
        maps  = 2;
        break;
    case '4':
        ascii = 0;
        maps  = 0;
        break;
    case '5':
        ascii = 0;
        maps  = 1;
        break;
    case '6':
        ascii = 0;
        maps  = 2;
        break;
    default:
        return NULL;
    }

    p = pnm_get_line(p, e, tmp);

    s = tmp;
    width = 0;
    while (isdigit(*s) && width >= 0)
        width = width * 10 + (*s++ - '0');
    while (*s == ' ')
        s++;
    height = 0;
    while (isdigit(*s) && height >= 0)
        height = height * 10 + (*s++ - '0');
    while (*s != '\0')
        s++;

    if (maps > 0) {
        p = pnm_get_line(p, e, tmp);
        s = tmp;
        deps = 0;
        while (isdigit(*s) && deps >= 0)
            deps = deps * 10 + (*s++ - '0');
    }

    if (width < 1 || height < 1 || deps < 1)
        return NULL;

    if ((result = malloc(width * height * 3 + 1)) == NULL)
        return NULL;

    memset(result, 0, width * height * 3 + 1);

    for (y = 0 ; y < height ; y++) {
        for (x = 0 ; x < width ; x++) {
            b = (maps == 2 ? 3 : 1);
            for (i = 0 ; i < b ; i++) {
                if (ascii) {
                    while (*s == '\0') {
                        if (p >= e)
                            break;
                        p = pnm_get_line(p, e, tmp);
                        s = tmp;
                    }
                    n = 0;
                    while (isdigit(*s) && n >= 0)
                        n = n * 10 + (*s++ - '0');
                    while (*s == ' ')
                        s++;
                } else {
                    if (p >= e)
                        break;
                    n = *(p++);
                }
                c[i] = n;
            }
            if (i < b)
                break;

            switch(maps) {
            case 0:        /* bitmap */
                if (c[0] == 0)
                    c[0] = c[1] = c[2] = 0;
                else
                    c[0] = c[1] = c[2] = 255;
                break;
            case 1:        /* graymap */
                c[0] = c[1] = c[2] = (c[0] * 255 / deps);
                break;
            case 2:        /* pixmap */
                c[0] = (c[0] * 255 / deps);
                c[1] = (c[1] * 255 / deps);
                c[2] = (c[2] * 255 / deps);
                break;
            }

            *(result + (y * width + x) * 3 + 0) = c[0];
            *(result + (y * width + x) * 3 + 1) = c[1];
            *(result + (y * width + x) * 3 + 2) = c[2];
        }
    }

    *psx = width;
    *psy = height;
    *pcomp = 3;
    *pstride = *psx * *pcomp;
    return result;
}
