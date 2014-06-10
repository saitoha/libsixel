#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <gd.h>

#define	PALVAL(n,a,m)	(((n) * (a) + ((m) / 2)) / (m))

typedef unsigned char BYTE;

typedef struct _SixNode {
	struct _SixNode *next;
	int	pal;
	int	sx;
	int	mx;
	BYTE *map;
} SixNode;

static FILE *out_fp = NULL;

static SixNode *node_top = NULL;
static SixNode *node_free = NULL;

static int save_pix = 0;
static int save_count = 0;
static char init_palet[gdMaxColors];
static int act_palet = (-1);

static long use_palet[gdMaxColors];
static BYTE conv_palet[gdMaxColors];

extern char *sixel_param;
extern char *sixel_gra;
extern int sixel_palfix;
extern char sixel_palinit[];
extern int sixel_palet[];

static void PutData(int ch)
{
    fputc(ch, out_fp);
}
static void PutStr(char *str)
{
    fputs(str, out_fp);
}
static void PutFmt(char *fmt, ...)
{
   va_list ap;
   va_start(ap, fmt);
   vfprintf(out_fp, fmt, ap);
   va_end(ap);
}

static void PutFlash()
{
    int n;

#ifdef	USE_VT240	// VT240 Max 255 ?
    while ( save_count > 255 ) {
	PutFmt("!%d%c", 255, save_pix);
	save_count -= 255;
    }
#endif

    if ( save_count > 3 ) {
	// DECGRI Graphics Repeat Introducer		! Pn Ch

	PutFmt("!%d%c", save_count, save_pix);

    } else {
	for ( n = 0 ; n < save_count ; n++ )
	    PutData(save_pix);
    }

    save_pix = 0;
    save_count = 0;
}
static void PutPixel(int pix)
{
    if ( pix < 0 || pix > 63 )
	pix = 0;

    pix += '?';

    if ( pix == save_pix ) {
	save_count++;
    } else {
	PutFlash();
	save_pix = pix;
	save_count = 1;
    }
}
static void PutPalet(gdImagePtr im, int pal)
{
    // DECGCI Graphics Color Introducer			# Pc ; Pu; Px; Py; Pz

    if ( init_palet[pal] == 0 ) {
    	PutFmt("#%d;2;%d;%d;%d", conv_palet[pal],
		PALVAL(gdImageRed  (im, pal), 100, gdRedMax  ), 
		PALVAL(gdImageGreen(im, pal), 100, gdGreenMax), 
		PALVAL(gdImageBlue (im, pal), 100, gdBlueMax ));
    	init_palet[pal] = 1;

    } else if ( act_palet != pal )
    	PutFmt("#%d", conv_palet[pal]);

    act_palet = pal;
}
static void PutCr()
{
    // DECGCR Graphics Carriage Return

    PutStr("$\n");
    // x = 0;
}
static void PutLf()
{
    // DECGNL Graphics Next Line

    PutStr("$-\n");
    // x = 0;
    // y += 6;
}

static void NodeFree()
{
    SixNode *np;

    while ( (np = node_free) != NULL ) {
	node_free = np->next;
	free(np);
    }
}
static void NodeDel(SixNode *np)
{
    SixNode *tp;

    if ( (tp = node_top) == np )
	node_top = np->next;

    else {
	while ( tp->next != NULL ) {
	    if ( tp->next == np ) {
		tp->next = np->next;
		break;
	    }
	    tp = tp->next;
	}
    }

    np->next = node_free;
    node_free = np;
}
static void NodeAdd(int pal, int sx, int mx, BYTE *map)
{
    SixNode *np, *tp, top;

    if ( (np = node_free) != NULL )
	node_free = np->next;
    else if ( (np = (SixNode *)malloc(sizeof(SixNode))) == NULL )
	return;

    np->pal = pal;
    np->sx = sx;
    np->mx = mx;
    np->map = map;

    top.next = node_top;
    tp = &top;

    while ( tp->next != NULL ) {
	if ( np->sx < tp->next->sx )
	    break;
	else if ( np->sx == tp->next->sx && np->mx > tp->next->mx )
	    break;
	tp = tp->next;
    }

    np->next = tp->next;
    tp->next = np;
    node_top = top.next;
}
static void NodeLine(int pal, int width, BYTE *map)
{
    int sx, mx, n;

    for ( sx = 0 ; sx < width ; sx++ ) {
	if ( map[sx] == 0 )
	    continue;

	for ( mx = sx + 1 ; mx < width ; mx++ ) {
	    if ( map[mx] != 0 )
		continue;

	    for ( n = 1 ; (mx + n) < width ; n++ ) {
		if ( map[mx + n] != 0 )
		    break;
	    }

	    if ( n >= 10 || (mx + n) >= width )
		break;
	    mx = mx + n - 1;
	}

	NodeAdd(pal, sx, mx, map);
	sx = mx - 1;
    }
}
static int PutNode(gdImagePtr im, int x, SixNode *np)
{
    PutPalet(im, np->pal);
	
    for ( ; x < np->sx ; x++ )
	PutPixel(0);

    for ( ; x < np->mx ; x++ )
	PutPixel(np->map[x]);

    PutFlash();

    return x;
}
static int PalUseCmp(const void *src, const void *dis)
{
    return use_palet[*((BYTE *)dis)] - use_palet[*((BYTE *)src)];
}
static int GetColIdx(gdImagePtr im, int col)
{
    int i, r, g, b, d;
    int red   = gdTrueColorGetRed(col);
    int green = gdTrueColorGetGreen(col);
    int blue  = gdTrueColorGetBlue(col);
    int idx   = (-1);
    int min   = 0xFFFFFF;    // 255 * 255 * 3 + 255 * 255 * 9 + 255 * 255 = 845325 = 0x000CE60D

    for ( i = 0 ; i < gdImageColorsTotal(im) ; i++ ) {
	if ( i == gdImageGetTransparent(im) )
	    continue;
	r = gdImageRed  (im, i) - red;
	g = gdImageGreen(im, i) - green;
	b = gdImageBlue (im, i) - blue;
	d = r * r * 3 + g * g * 9 + b * b;
	if ( min > d ) {
	    idx = i;
	    min = d;
	}
    }
    return idx;
}

void gdImageSixel(gdImagePtr im, FILE *out, int maxPalet, int optPalet)
{
    int x, y, i, n, c;
    int width, height;
    int len, pix, skip;
    int back = (-1);
    BYTE *map;
    SixNode *np;
    BYTE list[gdMaxColors];

    out_fp = out;

    width  = gdImageSX(im);
    height = gdImageSY(im);

    if ( maxPalet > gdMaxColors || maxPalet <= 0 )
	maxPalet = gdMaxColors;

    if ( gdImageTrueColor(im) ) {
	gdImageTrueColorToPalette(im, 1, maxPalet);
	sixel_palfix = 0;

    } else if ( gdImageColorsTotal(im) > maxPalet ) {
	gdImagePaletteToTrueColor(im);
	gdImageTrueColorToPalette(im, 1, maxPalet);
	sixel_palfix = 0;
    }

    maxPalet = gdImageColorsTotal(im);
    back = gdImageGetTransparent(im);
    len = maxPalet * width;

    if ( (map = (BYTE *)malloc(len)) == NULL )
	return;

    memset(map, 0, len);

    for ( n = 0 ; n < maxPalet ; n++ )
	conv_palet[n] = list[n] = n;

    memset(init_palet, 0, sizeof(init_palet));

    if ( sixel_palfix == 0 || optPalet != 0 ) { 

	// Pass 1 Palet count

        memset(use_palet, 0, sizeof(use_palet));
	skip = (height / 240) * 6;

    	for ( y = i = 0 ; y < height ; y++ ) {
	    for ( x = 0 ; x < width ; x++ ) {
	        pix = gdImagePalettePixel(im, x, y);
	        if ( pix >= 0 && pix < maxPalet && pix != back )
	    	    map[pix * width + x] |= (1 << i);
	    }

	    if ( ++i < 6 && (y + 1) < height )
	        continue;

	    for ( n = 0 ; n < maxPalet ; n++ )
	        NodeLine(n, width, map + n * width);

	    for ( x = 0 ; (np = node_top) != NULL ; ) {
	        if ( x > np->sx )
		    x = 0;

		use_palet[np->pal]++;

	        x = np->mx;
	        NodeDel(np);
	        np = node_top;

	        while ( np != NULL ) {
	    	    if ( np->sx < x ) {
		        np = np->next;
		        continue;
		    }

		    use_palet[np->pal]++;

	            x = np->mx;
		    NodeDel(np);
		    np = node_top;
	        }
	    }

	    i = 0;
    	    memset(map, 0, len);
	    y += skip;
	}

	qsort(list, maxPalet, sizeof(BYTE), PalUseCmp);

	for ( n = 0 ; n < maxPalet ; n++ )
	    conv_palet[list[n]] = n;

    } else {
	for ( n = 0 ; n < maxPalet && n < 16 ; n++ ) {
	    if ( sixel_palinit[n] != 1 )
		continue;
	    if ( (i = GetColIdx(im, sixel_palet[n])) < 0 )
		continue;
	    init_palet[i] = 1;
	    if ( conv_palet[i] != n ) {
		conv_palet[list[n]] = conv_palet[i];
		list[conv_palet[i]] = list[n];
		conv_palet[i] = n;
		list[n] = i;
	    }
	}
    }

/*************
    for ( n = 0 ; n < maxPalet ; n++ )
	fprintf(stderr, "%d %d=%d\n", n, list[n], conv_palet[list[n]]);
**************/

    PutStr("\033P");

    if ( sixel_param != NULL )
	PutStr(sixel_param);

    PutData('q');

    if ( sixel_gra != NULL )
	PutStr(sixel_gra);
    else
        PutFmt("\"1;1;%d;%d", width, height);

    PutData('\n');

    for ( y = i = 0 ; y < height ; y++ ) {
	for ( x = 0 ; x < width ; x++ ) {
	    pix = gdImagePalettePixel(im, x, y);
	    if ( pix >= 0 && pix < maxPalet && pix != back )
	    	map[pix * width + x] |= (1 << i);
	}

	if ( ++i < 6 && (y + 1) < height )
	    continue;

	for ( n = 0 ; n < maxPalet ; n++ )
	    NodeLine(n, width, map + n * width);

	for ( x = 0 ; (np = node_top) != NULL ; ) {
	    if ( x > np->sx ) {
		PutCr();
		x = 0;
	    }

	    x = PutNode(im, x, np);
	    NodeDel(np);
	    np = node_top;

	    while ( np != NULL ) {
	    	if ( np->sx < x ) {
		    np = np->next;
		    continue;
		}

		x = PutNode(im, x, np);
		NodeDel(np);
		np = node_top;
	    }
	}

	PutLf();

	i = 0;
    	memset(map, 0, len);
    }

    PutStr("\033\\");

    NodeFree();
    free(map);
}
