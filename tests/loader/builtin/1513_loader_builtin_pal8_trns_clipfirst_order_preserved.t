#!/bin/sh
# Verify clipfirst ordering remains stable for PAL8 transparent geometry.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}


echo "1..1"
set -v

input_png="${TOP_SRCDIR}/tests/data/inputs/formats/pal8-trns-key0.png"
clipfirst_log=""
scalefirst_log=""
clipfirst_out=""
scalefirst_out=""
clipfirst_edge_ok=0
scalefirst_edge_ok=0

clipfirst_log=$(${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    -v -Lbuiltin:cms_engine=none! -d fs:scan=raster \
    -c2x1+1+0 -w7 -h3 -r nearest "${input_png}" 2>&1 >/dev/null) || {
    echo "not ok 1 - clip then scale render failed"
    exit 0
}

scalefirst_log=$(${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    -v -Lbuiltin:cms_engine=none! -d fs:scan=raster \
    -w7 -h3 -r nearest -c2x1+1+0 "${input_png}" 2>&1 >/dev/null) || {
    echo "not ok 1 - scale then clip render failed"
    exit 0
}

clipfirst_out=$(${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    -Lbuiltin:cms_engine=none! -d fs:scan=raster \
    -c2x1+1+0 -w7 -h3 -r nearest "${input_png}") || {
    echo "not ok 1 - clip then scale sixel output failed"
    exit 0
}

scalefirst_out=$(${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    -Lbuiltin:cms_engine=none! -d fs:scan=raster \
    -w7 -h3 -r nearest -c2x1+1+0 "${input_png}") || {
    echo "not ok 1 - scale then clip sixel output failed"
    exit 0
}

case "${clipfirst_log}" in
    *"load -> clip"*\
*"clip -> colorspace(pre)"*)
        clipfirst_edge_ok=1
        ;;
esac

case "${scalefirst_log}" in
    *"load -> colorspace(pre)"*\
*"scale -> clip"*)
        scalefirst_edge_ok=1
        ;;
esac

test "${clipfirst_edge_ok}" = 1 || {
    echo "not ok 1 - clipfirst planner edges are missing"
    exit 0
}

test "${scalefirst_edge_ok}" = 1 || {
    echo "not ok 1 - scalefirst planner edges are missing"
    exit 0
}

test "${clipfirst_out}" != "${scalefirst_out}" || {
    echo "not ok 1 - clipfirst and scalefirst outputs are unexpectedly identical"
    exit 0
}

echo "ok 1 - clipfirst ordering remains effective for pal8 transparent input"
exit 0
