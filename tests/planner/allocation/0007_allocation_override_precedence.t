#!/bin/sh
# Policy: docs/threading/encoder.md
# An explicit low-budget cap and geometry outrank conflicting environments.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    echo "1..0 # SKIP img2sixel is disabled"
    set -v
    exit 0
}
test "${SIXEL_ENABLE_THREADS-0}" = 1 || {
    echo "1..0 # SKIP threading is disabled"
    set -v
    exit 0
}
echo "1..1"
set -v

trace=$(set +xv; SIXEL_TRACE_TOPIC=dither_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -Goff -Efast -=4 -p16 \
    --env SIXEL_DITHER_PARALLEL_THREADS_MAX=2 \
    --env SIXEL_DITHER_PARALLEL_BAND_WIDTH=18 \
    --env SIXEL_DITHER_PARALLEL_BAND_OVERWRAP=6 \
    -d 'fs:threads_max=1:band_width=9:band_overwrap=0' \
    "${TOP_SRCDIR}/tests/data/inputs/snake_64.png" 2>&1 >/dev/null) || {
    echo "not ok 1 - explicit allocation conversion failed"
    exit 0
}

case "${trace}" in
    *LSXDTH1\|*band_height=12\|*band_overlap=0\|*dither_threads=1\|encode_threads=3\|*allocation_mode=pipeline\|*) true ;;
    *) false ;;
esac || {
    echo "not ok 1 - explicit allocation lost precedence"
    exit 0
}

echo "ok 1 - explicit allocation outranks environment and automatic defaults"
exit 0
