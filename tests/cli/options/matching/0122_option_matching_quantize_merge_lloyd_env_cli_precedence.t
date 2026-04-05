#!/bin/sh
# TAP test verifying merge_lloyd follows env/CLI precedence.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}


echo "1..1"
set -v

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env "SIXEL_PALETTE_FINAL_MERGE_ADDITIONAL_LLOYD_ITER_COUNT=4" \
    -Qauto \
    "${TOP_SRCDIR}/tests/data/inputs/small.ppm" \
    -o/dev/null >/dev/null 2>&1 || {
    echo "not ok" 1 - "env-only merge_lloyd=4 was rejected"
    exit 0
}

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    -Qauto:merge_lloyd=0 \
    "${TOP_SRCDIR}/tests/data/inputs/small.ppm" \
    -o/dev/null >/dev/null 2>&1 || {
    echo "not ok" 1 - "cli-only merge_lloyd=0 was rejected"
    exit 0
}

msg=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env "SIXEL_PALETTE_FINAL_MERGE_ADDITIONAL_LLOYD_ITER_COUNT=4" \
    -Qauto:merge_lloyd=31 \
    "${TOP_SRCDIR}/tests/data/inputs/small.ppm" \
    -o/dev/null 2>&1) && {
    echo "not ok" 1 - "invalid CLI merge_lloyd unexpectedly ignored"
    exit 0
}

test "${msg#*-Q merge_lloyd must be 0 or in range 1-30.*}" != "${msg}" || {
    echo "not ok" 1 - "missing invalid CLI merge_lloyd diagnostic"
    exit 0
}

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env "SIXEL_PALETTE_FINAL_MERGE_ADDITIONAL_LLOYD_ITER_COUNT=80" \
    -Qauto:merge_lloyd=0 \
    "${TOP_SRCDIR}/tests/data/inputs/small.ppm" \
    -o/dev/null >/dev/null 2>&1 || {
    echo "not ok" 1 - "valid CLI merge_lloyd did not override invalid env"
    exit 0
}

echo "ok" 1 - "merge_lloyd follows env/CLI precedence"
exit 0
