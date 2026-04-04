#!/bin/sh
# TAP test verifying kmedoids clarans_local accepts env/CLI values and keeps CLI priority.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}


echo "1..1"
set -v

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env "SIXEL_PALETTE_KMEDOIDS_CLARANS_LOCAL=1" \
    -Qkmedoids \
    "${TOP_SRCDIR}/tests/data/inputs/small.ppm" \
    -o/dev/null >/dev/null 2>&1 || {
    echo "not ok" 1 - "env-only clarans_local=1 was rejected"
    exit 0
}

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    -Qkmedoids:clarans_local=32 \
    "${TOP_SRCDIR}/tests/data/inputs/small.ppm" \
    -o/dev/null >/dev/null 2>&1 || {
    echo "not ok" 1 - "cli-only clarans_local upper bound was rejected"
    exit 0
}

msg=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env "SIXEL_PALETTE_KMEDOIDS_CLARANS_LOCAL=1" \
    -Qkmedoids:clarans_local=0 \
    "${TOP_SRCDIR}/tests/data/inputs/small.ppm" \
    -o/dev/null 2>&1) && {
    echo "not ok" 1 - "invalid CLI clarans_local unexpectedly ignored in favor of env"
    exit 0
}

test "${msg#*-Q clarans_local must be in range 1-32.*}" != "${msg}" || {
    echo "not ok" 1 - "missing invalid CLI clarans_local diagnostic"
    exit 0
}

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env "SIXEL_PALETTE_KMEDOIDS_CLARANS_LOCAL=0" \
    -Qkmedoids:clarans_local=32 \
    "${TOP_SRCDIR}/tests/data/inputs/small.ppm" \
    -o/dev/null >/dev/null 2>&1 || {
    echo "not ok" 1 - "valid CLI clarans_local did not override invalid env"
    exit 0
}

echo "ok" 1 - "kmedoids clarans_local follows env/CLI acceptance and CLI priority"
exit 0
