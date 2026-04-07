#!/bin/sh
# TAP test verifying merge short keys work on quantize models.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    -Qauto:g=ward:o=1.2:l=0 \
    "${TOP_SRCDIR}/tests/data/inputs/small.ppm" \
    -o/dev/null >/dev/null 2>&1 || {
    echo "not ok" 1 - "auto merge short keys were rejected"
    exit 0
}

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    -Qkmeans:s=1:restarts=1:g=none:o=1.4:l=3 \
    "${TOP_SRCDIR}/tests/data/inputs/small.ppm" \
    -o/dev/null >/dev/null 2>&1 || {
    echo "not ok" 1 - "kmeans merge short keys were rejected"
    exit 0
}

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    -Qmedoids:a=sample:s=1:g=auto:o=2.0:l=5 \
    "${TOP_SRCDIR}/tests/data/inputs/small.ppm" \
    -o/dev/null >/dev/null 2>&1 || {
    echo "not ok" 1 - "medoids merge short keys were rejected"
    exit 0
}

echo "ok" 1 - "merge short keys are accepted on auto/kmeans/medoids"
exit 0
