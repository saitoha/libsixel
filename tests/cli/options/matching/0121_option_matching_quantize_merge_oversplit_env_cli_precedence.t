#!/bin/sh
# TAP test verifying merge_oversplit follows env/CLI precedence.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}


echo "1..1"
set -v

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env "SIXEL_PALETTE_OVERSPLIT_FACTOR=2.0" \
    -Qauto \
    "${TOP_SRCDIR}/tests/data/inputs/small.ppm" \
    -o/dev/null >/dev/null 2>&1 || {
    echo "not ok" 1 - "env-only merge_oversplit=2.0 was rejected"
    exit 0
}

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    -Qauto:merge_oversplit=1.2 \
    "${TOP_SRCDIR}/tests/data/inputs/small.ppm" \
    -o/dev/null >/dev/null 2>&1 || {
    echo "not ok" 1 - "cli-only merge_oversplit was rejected"
    exit 0
}

msg=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env "SIXEL_PALETTE_OVERSPLIT_FACTOR=2.0" \
    -Qauto:merge_oversplit=3.5 \
    "${TOP_SRCDIR}/tests/data/inputs/small.ppm" \
    -o/dev/null 2>&1) && {
    echo "not ok" 1 - "invalid CLI merge_oversplit unexpectedly ignored"
    exit 0
}

test "${msg#*-Q merge_oversplit must be in range 1.0-3.0.*}" != "${msg}" || {
    echo "not ok" 1 - "missing invalid CLI merge_oversplit diagnostic"
    exit 0
}

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env "SIXEL_PALETTE_OVERSPLIT_FACTOR=9.0" \
    -Qauto:merge_oversplit=1.2 \
    "${TOP_SRCDIR}/tests/data/inputs/small.ppm" \
    -o/dev/null >/dev/null 2>&1 || {
    echo "not ok" 1 - "valid CLI merge_oversplit did not override invalid env"
    exit 0
}

echo "ok" 1 - "merge_oversplit follows env/CLI precedence"
exit 0
