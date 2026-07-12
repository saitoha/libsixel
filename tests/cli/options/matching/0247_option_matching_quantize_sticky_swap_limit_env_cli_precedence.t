#!/bin/sh
# TAP test verifying sticky swap_limit accepts env/CLI values with CLI priority.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v

msg=''

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env "SIXEL_PALETTE_STICKY_SWAP_LIMIT=2" \
    -Qsticky \
    "${TOP_SRCDIR}/tests/data/inputs/small.ppm" \
    -o/dev/null >/dev/null 2>&1 || {
    echo "not ok" 1 - "env-only sticky swap_limit=2 was rejected"
    exit 0
}

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    -Qsticky:swap_limit=0 \
    "${TOP_SRCDIR}/tests/data/inputs/small.ppm" \
    -o/dev/null >/dev/null 2>&1 || {
    echo "not ok" 1 - "cli-only sticky swap_limit=0 was rejected"
    exit 0
}

msg=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env "SIXEL_PALETTE_STICKY_SWAP_LIMIT=2" \
    -Qsticky:swap_limit=257 \
    "${TOP_SRCDIR}/tests/data/inputs/small.ppm" \
    -o/dev/null 2>&1) && {
    echo "not ok" 1 - "invalid CLI sticky swap_limit unexpectedly succeeded"
    exit 0
}

test "${msg#*-Q swap_limit must be 0 or in range 1-256.*}" != "${msg}" || {
    echo "not ok" 1 - "missing invalid sticky swap_limit diagnostic"
    exit 0
}

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env "SIXEL_PALETTE_STICKY_SWAP_LIMIT=invalid" \
    -Qsticky:swap_limit=2 \
    "${TOP_SRCDIR}/tests/data/inputs/small.ppm" \
    -o/dev/null >/dev/null 2>&1 || {
    echo "not ok" 1 - "valid CLI sticky swap_limit did not override env"
    exit 0
}

echo "ok" 1 - "sticky swap_limit follows env/CLI acceptance and CLI priority"
exit 0
