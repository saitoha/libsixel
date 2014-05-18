#!/bin/sh

CFLAGS="`pkg-config gdk-pixbuf-2.0 --cflags` `pkg-config libcurl --cflags` -DUSE_GDK_PIXBUF -DUSE_LIBCURL" \
LIBS="`pkg-config gdk-pixbuf-2.0 --libs` `pkg-config libcurl --libs`" \
./configure ${@}
make
