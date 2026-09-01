#!/bin/sh
# TAP test verifying center swap_min_gain keeps CLI priority over env.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}


echo "1..1"
set -v

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env "SIXEL_PALETTE_KCENTER_SWAP_MIN_GAIN=0.10" \
    -Qcenter \
    "${TOP_SRCDIR}/tests/data/inputs/small.ppm" \
    -o/dev/null >/dev/null 2>&1 || {
    echo "not ok" 1 - "env-only center swap_min_gain=0.10 was rejected"
    exit 0
}

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    -Qcenter:swap_min_gain=0.05 \
    "${TOP_SRCDIR}/tests/data/inputs/small.ppm" \
    -o/dev/null >/dev/null 2>&1 || {
    echo "not ok" 1 - "cli-only center swap_min_gain=0.05 was rejected"
    exit 0
}

msg=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env "SIXEL_PALETTE_KCENTER_SWAP_MIN_GAIN=0.10" \
    -Qcenter:swap_min_gain=9.0 \
    "${TOP_SRCDIR}/tests/data/inputs/small.ppm" \
    -o/dev/null 2>&1) && {
    echo "not ok" 1 - "invalid CLI swap_min_gain unexpectedly ignored in favor of env"
    exit 0
}

test "${msg#*swap_min_gain*}" != "${msg}" || {
    echo "not ok" 1 - "missing invalid CLI center swap_min_gain diagnostic"
    exit 0
}

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env "SIXEL_PALETTE_KCENTER_SWAP_MIN_GAIN=9.0" \
    -Qcenter:swap_min_gain=0.02 \
    "${TOP_SRCDIR}/tests/data/inputs/small.ppm" \
    -o/dev/null >/dev/null 2>&1 || {
    echo "not ok" 1 - "valid CLI center swap_min_gain did not override invalid env"
    exit 0
}

echo "ok" 1 - "center swap_min_gain follows env/CLI acceptance and CLI priority"
exit 0
