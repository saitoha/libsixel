#!/bin/sh
# Verify palette diagnostics expose the point population after hard binning.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v

trace=$(set +xv; SIXEL_TRACE_TOPIC=palette_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --threads=1 \
    -Qauto:binning_policy=hard -Qkmeans:binbits=6 -dnone -p16 "-~none" \
    -Lbuiltin -ldisable \
    "${TOP_SRCDIR}/tests/data/inputs/small.ppm" 2>&1 >/dev/null) || {
    echo "not ok 1 - hard-binning diagnostics failed"
    exit 0
}

test "${trace#*LSXBSTAT1|bits=6|source_points=*|effective_points=*}" \
    != "${trace}" || {
    echo "not ok 1 - hard-binning diagnostics omit effective points"
    exit 0
}

echo "ok 1 - hard-binning diagnostics expose effective points"
exit 0
