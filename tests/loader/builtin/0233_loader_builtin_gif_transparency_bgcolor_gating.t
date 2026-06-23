#!/bin/sh
# Verify builtin GIF transparency keeps keycolor unless bgcolor is provided.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v

input_gif="${TOP_SRCDIR}/tests/data/inputs/formats/gif-transparent-anim-dispose2.gif"
keycolor_header="$(printf '\033P0;0q')"
default_output=''
bgcolor_output=''
default_line=''
bgcolor_line=''
nl='
'

default_output=$(
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --env SIXEL_THREADS=1 \
              -Lbuiltin! \
              -ldisable -S -T 1 -d fs:scan=raster \
              "${input_gif}"
) || {
    echo "not ok" 1 - "builtin transparent GIF default decode failed"
    exit 0
}

bgcolor_output=$(
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --env SIXEL_THREADS=1 \
              --env SIXEL_BGCOLOR=white \
              -Lbuiltin! \
              -ldisable -S -T 1 -d fs:scan=raster \
              "${input_gif}"
) || {
    echo "not ok" 1 - "builtin transparent GIF with bgcolor decode failed"
    exit 0
}

default_line=${default_output%%"${nl}"*}
bgcolor_line=${bgcolor_output%%"${nl}"*}

case "${default_line}" in
    *"${keycolor_header}"*) ;;
    *)
        echo "not ok" 1 - "builtin GIF default output is missing keycolor"
        exit 0
        ;;
esac

case "${bgcolor_line}" in
    *"${keycolor_header}"*)
        echo "not ok" 1 - "builtin GIF bgcolor output unexpectedly kept keycolor"
        exit 0
        ;;
    *) ;;
esac

echo "ok" 1 - "builtin GIF transparency keycolor is gated by bgcolor"
exit 0
