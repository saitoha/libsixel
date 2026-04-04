#!/bin/sh
# TAP test verifying medoids clarans_neighbors accepts env/CLI values and keeps CLI priority.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}


echo "1..1"
set -v

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env "SIXEL_PALETTE_KMEDOIDS_CLARANS_NEIGHBORS=0" \
    -Qmedoids \
    "${TOP_SRCDIR}/tests/data/inputs/small.ppm" \
    -o/dev/null >/dev/null 2>&1 || {
    echo "not ok" 1 - "env-only clarans_neighbors=0 was rejected"
    exit 0
}

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    -Qmedoids:clarans_neighbors=5000000 \
    "${TOP_SRCDIR}/tests/data/inputs/small.ppm" \
    -o/dev/null >/dev/null 2>&1 || {
    echo "not ok" 1 - "cli-only clarans_neighbors upper bound was rejected"
    exit 0
}

msg=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env "SIXEL_PALETTE_KMEDOIDS_CLARANS_NEIGHBORS=0" \
    -Qmedoids:clarans_neighbors=5000001 \
    "${TOP_SRCDIR}/tests/data/inputs/small.ppm" \
    -o/dev/null 2>&1) && {
    echo "not ok" 1 - "invalid CLI clarans_neighbors unexpectedly ignored in favor of env"
    exit 0
}

test "${msg#*-Q clarans_neighbors must be 0 or in range 1-5000000.*}" != "${msg}" || {
    echo "not ok" 1 - "missing invalid CLI clarans_neighbors diagnostic"
    exit 0
}

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env "SIXEL_PALETTE_KMEDOIDS_CLARANS_NEIGHBORS=5000001" \
    -Qmedoids:clarans_neighbors=5000000 \
    "${TOP_SRCDIR}/tests/data/inputs/small.ppm" \
    -o/dev/null >/dev/null 2>&1 || {
    echo "not ok" 1 - "valid CLI clarans_neighbors did not override invalid env"
    exit 0
}

echo "ok" 1 - "medoids clarans_neighbors follows env/CLI acceptance and CLI priority"
exit 0
