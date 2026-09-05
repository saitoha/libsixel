#!/bin/sh
# Verify top-level soft binning preserves the legacy alias output.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v

legacy=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --threads=1 \
    -Qkmeans:binning=soft -dnone -p16 "-~none" -Lbuiltin -ldisable \
    "${TOP_SRCDIR}/tests/data/inputs/small.ppm") || {
    echo "not ok 1 - legacy soft binning encode failed"
    exit 0
}
top_level=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --threads=1 \
    -Qkmeans --palette-binning=soft -dnone -p16 "-~none" \
    -Lbuiltin -ldisable \
    "${TOP_SRCDIR}/tests/data/inputs/small.ppm") || {
    echo "not ok 1 - top-level soft binning encode failed"
    exit 0
}

test "${top_level}" = "${legacy}" || {
    echo "not ok 1 - top-level and legacy soft binning output differ"
    exit 0
}

echo "ok 1 - top-level soft binning preserves legacy alias output"
exit 0
