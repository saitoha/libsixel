#!/bin/sh
# Verify exact binning remains rejected until its filter is implemented.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v

status=0
message=$(set +xv; SIXEL_TRACE_TOPIC=palette_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --threads=4 \
    --palette-binning=exact -Qkmeans -dnone -p16 "-~none" \
    -Lbuiltin -ldisable \
    "${TOP_SRCDIR}/tests/data/inputs/small.ppm" 2>&1 >/dev/null) || status=$?

test "${status}" -eq 2 || {
    echo "not ok 1 - deferred exact policy exit status mismatch"
    exit 0
}

test "${message#*not executable yet*}" != "${message}" || {
    echo "not ok 1 - deferred exact policy diagnostic is missing"
    exit 0
}

test "${message#*LSXPFB1*}" = "${message}" || {
    echo "not ok 1 - deferred exact policy entered sampling fallback"
    exit 0
}

echo "ok 1 - exact palette binning is explicitly deferred"
exit 0
