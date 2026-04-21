#!/bin/sh
# TAP test: coregraphics animation start frame accepts positive indexes.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}

test "${HAVE_COREGRAPHICS-}" = 1 || {
    printf "1..0 # SKIP coregraphics support is disabled in this build\n";
    exit 0
}


echo "1..1"
set -v
default_out=""
positive_out=""

default_out=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    -Lcoregraphics! -ldisable -p 2 \
    "${TOP_SRCDIR}/tests/data/inputs/small.gif" 2>/dev/null) || {
    echo "not ok" 1 - "baseline coregraphics animation decode failed"
    exit 0
}

positive_out=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --start-frame=1 -Lcoregraphics! -ldisable -p 2 \
    "${TOP_SRCDIR}/tests/data/inputs/small.gif" 2>/dev/null) || {
    echo "not ok" 1 - "coregraphics decode with positive start frame failed"
    exit 0
}

test "${default_out}" != "${positive_out}" || {
    echo "not ok" 1 - "positive start frame did not change coregraphics output"
    exit 0
}

echo "ok" 1 - "coregraphics positive start frame is applied"
exit 0
