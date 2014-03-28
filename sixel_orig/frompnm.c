#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <gd.h>

typedef unsigned char BYTE;

static char *GetLine(BYTE *p, BYTE *e, BYTE *line)
{
    int n;

    do {
        for ( n = 0 ; p < e && *p >= ' '; p++ ) {
	    if ( n < 255 )
	        line[n++] = *p;
        }

        if ( p < e && *p == '\n' )
	    p++;

        line[n] = '\0';

    } while ( line[0] == '#' );

    return p;
}
gdImagePtr gdImageCreateFromPnmPtr(int len, BYTE *p)
{
    int n, i, b, x, y, c[3];
    int ascii, maps;
    int width, height, deps;
    gdImagePtr im;
    BYTE *s, *e, tmp[256];

    width = height = 0;
    deps = 1;

    e = p + len;
    p = GetLine(p, e, tmp);

    if ( tmp[0] != 'P' )
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

    p = GetLine(p, e, tmp);

    s = tmp;
    width = atoi(s);
    while ( isdigit(*s) )
	s++;
    while ( *s == ' ' )
	s++;
    height = atoi(s);
    while ( *s != '\0' )
	s++;

    if ( maps > 0 ) {
	p = GetLine(p, e, tmp);
	deps = atoi(tmp);
    }

    if ( width < 1 || height < 1 || deps < 1 )
	return NULL;

    if ( (im = gdImageCreateTrueColor(width, height)) == NULL )
	return NULL;

    gdImageFill(im, 0, 0, gdTrueColor(0, 0, 0));

    for ( y = 0 ; y < height ; y++ ) {
	for ( x = 0 ; x < width ; x++ ) {
	    b = (maps == 2 ? 3 : 1);
	    for ( i = 0 ; i < b ; i++ ) {
	        if ( ascii ) {
		    while ( *s == '\0' ) {
		        if ( p >= e )
			    break;
		        p = GetLine(p, e, tmp);
		        s = tmp;
		    }
		    n = atoi(s);
		    while ( isdigit(*s) )
		        s++;
		    while ( *s == ' ' )
		        s++;
	        } else {
		    if ( p >= e )
		        break;
		    n = *(p++);
	        }
		c[i] = n;
	    }
	    if ( i < b )
		break;

	    switch(maps) {
	    case 0:	// bitmap
		if ( c[0] == 0 )
		    c[0] = c[1] = c[2] = 0;
		else
		    c[0] = c[1] = c[2] = 255;
		break;
	    case 1:	// graymap
		c[0] = c[1] = c[2] = (c[0] * 255 / deps);
		break;
	    case 2:	// pixmap
		c[0] = (c[0] * 255 / deps);
		c[1] = (c[1] * 255 / deps);
		c[2] = (c[2] * 255 / deps);
		break;
	    }

	    gdImageSetPixel(im, x, y, gdTrueColor(c[0], c[1], c[2]));
	}
    }

    return im;
}
