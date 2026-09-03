#!/bin/sh
# Verify the effective pipeline band height selected by the environment.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}
test "${SIXEL_ENABLE_THREADS-0}" = 1 || {
    printf "1..0 # SKIP threading is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v

input_image="${TOP_SRCDIR}/tests/data/inputs/snake_64.png"

width_trace=$(set +xv; SIXEL_TRACE_TOPIC=dither_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --env "SIXEL_THREADS=6" \
    --env "SIXEL_DITHER_PARALLEL_BAND_WIDTH=9" -d fs \
    "${input_image}" 2>&1 >/dev/null) || {
    echo "not ok" 1 - "band width environment conversion failed"
    exit 0
}

test "${width_trace#*LSXDTH1|*diffuse=fs|*band_height=12*}" != \
    "${width_trace}" || {
    echo "not ok" 1 - "band width was not rounded to six rows"
    exit 0
}

negative_trace=$(set +xv; SIXEL_TRACE_TOPIC=dither_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --env "SIXEL_THREADS=6" \
    --env "SIXEL_DITHER_PARALLEL_BAND_WIDTH=-1" -d fs \
    "${input_image}" 2>&1 >/dev/null) || {
    echo "not ok" 1 - "negative band width conversion failed"
    exit 0
}

test "${negative_trace#*LSXDTH1|*diffuse=fs|*band_height=18*}" != \
    "${negative_trace}" || {
    echo "not ok" 1 - "negative band width changed the default"
    exit 0
}

echo "ok" 1 - "band width environment controls effective geometry"
exit 0
