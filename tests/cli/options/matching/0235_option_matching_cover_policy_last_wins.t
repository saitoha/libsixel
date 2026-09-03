#!/bin/sh
# TAP test verifying repeated -a options follow argument-order last-wins.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v

msg=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env SIXEL_TRACE_TOPIC=palette_contract \
    -p 16 -Qheckbert -a off -a corners \
    "${TOP_SRCDIR}/tests/data/inputs/small.ppm" \
    -o/dev/null 2>&1) || {
    echo "not ok" 1 - "late corners cover encode failed"
    exit 0
}

test "${msg#*LSXCOV1|policy=corners*}" != "${msg}" || {
    echo "not ok" 1 - "late corners did not override early off"
    exit 0
}

msg=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env SIXEL_TRACE_TOPIC=palette_contract \
    -p 16 -Qheckbert -a corners -a off \
    "${TOP_SRCDIR}/tests/data/inputs/small.ppm" \
    -o/dev/null 2>&1) || {
    echo "not ok" 1 - "late off cover encode failed"
    exit 0
}

test "${msg#*LSXCOV1|policy=off*}" != "${msg}" || {
    echo "not ok" 1 - "late off did not override early corners"
    exit 0
}

echo "ok" 1 - "-a cover policy uses argument-order last-wins"
exit 0
