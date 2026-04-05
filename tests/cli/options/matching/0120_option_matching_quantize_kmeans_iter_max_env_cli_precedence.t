#!/bin/sh
# TAP test verifying kmeans iter_max follows env/CLI precedence.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}


echo "1..1"
set -v

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env "SIXEL_PALETTE_KMEANS_ITER_COUNT_MAX=5" \
    -Qkmeans \
    "${TOP_SRCDIR}/tests/data/inputs/small.ppm" \
    -o/dev/null >/dev/null 2>&1 || {
    echo "not ok" 1 - "env-only iter_max=5 was rejected"
    exit 0
}

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    -Qkmeans:iter_max=100 \
    "${TOP_SRCDIR}/tests/data/inputs/small.ppm" \
    -o/dev/null >/dev/null 2>&1 || {
    echo "not ok" 1 - "cli-only iter_max upper bound was rejected"
    exit 0
}

msg=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env "SIXEL_PALETTE_KMEANS_ITER_COUNT_MAX=5" \
    -Qkmeans:iter_max=101 \
    "${TOP_SRCDIR}/tests/data/inputs/small.ppm" \
    -o/dev/null 2>&1) && {
    echo "not ok" 1 - "invalid CLI iter_max unexpectedly ignored"
    exit 0
}

test "${msg#*-Q iter_max must be in range 1-100.*}" != "${msg}" || {
    echo "not ok" 1 - "missing invalid CLI iter_max diagnostic"
    exit 0
}

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env "SIXEL_PALETTE_KMEANS_ITER_COUNT_MAX=0" \
    -Qkmeans:iter_max=100 \
    "${TOP_SRCDIR}/tests/data/inputs/small.ppm" \
    -o/dev/null >/dev/null 2>&1 || {
    echo "not ok" 1 - "valid CLI iter_max did not override invalid env"
    exit 0
}

echo "ok" 1 - "kmeans iter_max follows env/CLI precedence"
exit 0
