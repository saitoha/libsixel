#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <gd.h>

typedef unsigned char BYTE;

extern void gdImageSixel(gdImagePtr gd, FILE *out, int maxPalet, int optPalet);

extern gdImagePtr gdImageCreateFromSixelPtr(int len, BYTE *p);
extern gdImagePtr gdImageCreateFromPnmPtr(int len, BYTE *p);

#define	FMT_GIF	    0
#define	FMT_PNG	    1
#define	FMT_BMP	    2
#define	FMT_JPG	    3
#define	FMT_TGA	    4
#define	FMT_WBMP    5
#define	FMT_TIFF    6
#define	FMT_SIXEL   7
#define	FMT_PNM	    8
#define	FMT_GD2     9

static int FileFmt(int len, BYTE *data)
{
    if ( memcmp("TRUEVISION", data + len - 18, 10) == 0 )
	return FMT_TGA;

    if ( memcmp("GIF", data, 3) == 0 )
	return FMT_GIF;

    if ( memcmp("\x89\x50\x4E\x47\x0D\x0A\x1A\x0A", data, 8) == 0 )
	return FMT_PNG;

    if ( memcmp("BM", data, 2) == 0 )
	return FMT_BMP;

    if ( memcmp("\xFF\xD8", data, 2) == 0 )
	return FMT_JPG;

    if ( memcmp("\x00\x00", data, 2) == 0 )
	return FMT_WBMP;

    if ( memcmp("\x4D\x4D", data, 2) == 0 )
	return FMT_TIFF;

    if ( memcmp("\x49\x49", data, 2) == 0 )
	return FMT_TIFF;

    if ( memcmp("\033P", data, 2) == 0 )
	return FMT_SIXEL;

    if ( data[0] == 0x90  && (data[len-1] == 0x9C || data[len-2] == 0x9C) )
	return FMT_SIXEL;

    if ( data[0] == 'P' && data[1] >= '1' && data[1] <= '6' )
	return FMT_PNM;

    if ( memcmp("gd2", data, 3) == 0 )
	return FMT_GD2;

    return (-1);
}
static int ConvSixel(char *filename, int maxPalet, int optPalet)
{
    int n, len, max;
    FILE *fp = stdin;
    BYTE *data;
    gdImagePtr im = NULL;

    if ( filename != NULL && (fp = fopen(filename, "r")) == NULL )
	return (-1);

    len = 0;
    max = 64 * 1024;

    if ( (data = (BYTE *)malloc(max)) == NULL )
	return (-1);

    for ( ; ; ) {
	if ( (max - len) < 4096 ) {
	    max *= 2;
    	    if ( (data = (BYTE *)realloc(data, max)) == NULL )
		return (-1);
	}
	if ( (n = fread(data + len, 1, 4096, fp)) <= 0 )
	    break;
	len += n;
    }

    if ( fp != stdout )
    	fclose(fp);

    switch(FileFmt(len, data)) {
	case FMT_GIF:
    	    im = gdImageCreateFromGifPtr(len, data);
	    break;
	case FMT_PNG:
	    im = gdImageCreateFromPngPtr(len, data);
	    break;
	case FMT_BMP:
    	    im = gdImageCreateFromBmpPtr(len, data);
	    break;
	case FMT_JPG:
    	    im = gdImageCreateFromJpegPtrEx(len, data, 1);
	    break;
	case FMT_TGA:
    	    im = gdImageCreateFromTgaPtr(len, data);
	    break;
	case FMT_WBMP:
    	    im = gdImageCreateFromWBMPPtr(len, data);
	    break;
	case FMT_TIFF:
    	    im = gdImageCreateFromTiffPtr(len, data);
	    break;
	case FMT_SIXEL:
    	    im = gdImageCreateFromSixelPtr(len, data);
	    break;
	case FMT_PNM:
    	    im = gdImageCreateFromPnmPtr(len, data);
	    break;
	case FMT_GD2:
    	    im = gdImageCreateFromGd2Ptr(len, data);
	    break;
    }

    free(data);

    if ( im == NULL )
	return 1;

    if ( maxPalet < 2 )
	maxPalet = 2;
    else if ( maxPalet > gdMaxColors )
	maxPalet = gdMaxColors;

    gdImageSixel(im, stdout, maxPalet, optPalet);

    return 0;
}

int main(int ac, char *av[])
{
    int n;
    int mx = 1;
    int maxPalet = gdMaxColors;
    int optPalet = 0;

    for ( ; ; ) {
	while ( (n = getopt(ac, av, "p:c")) != EOF ) {
	    switch(n) {
	    case 'p':
		maxPalet = atoi(optarg);
		break;
	    case 'c':
		optPalet = 1;
		break;
	    default:
		fprintf(stderr, "Usage: %s [-p MaxPalet] [-c] <file name...>\n", av[0]);
		exit(0);
	    }
	}
	if ( optind >= ac )
	    break;
	av[mx++] = av[optind++];
    }

    if ( mx <= 1 ) {
	ConvSixel(NULL, maxPalet, optPalet);

    } else {
    	for ( n = 1 ; n < mx ; n++ )
	    ConvSixel(av[n], maxPalet, optPalet);
    }

    return 0;
}
