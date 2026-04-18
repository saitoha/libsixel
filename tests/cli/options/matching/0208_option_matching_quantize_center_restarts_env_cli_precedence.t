#!/bin/sh
# TAP test verifying center restarts accepts env/CLI values and keeps CLI priority.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}


echo "1..1"
set -v

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env "SIXEL_PALETTE_KCENTER_RESTARTS=2" \
    -Qcenter \
    "${TOP_SRCDIR}/tests/data/inputs/small.ppm" \
    -o/dev/null >/dev/null 2>&1 || {
    echo "not ok" 1 - "env-only center restarts=2 was rejected"
    exit 0
}

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    -Qcenter:restarts=32 \
    "${TOP_SRCDIR}/tests/data/inputs/small.ppm" \
    -o/dev/null >/dev/null 2>&1 || {
    echo "not ok" 1 - "cli-only center restarts upper bound was rejected"
    exit 0
}

msg=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env "SIXEL_PALETTE_KCENTER_RESTARTS=2" \
    -Qcenter:restarts=0 \
    "${TOP_SRCDIR}/tests/data/inputs/small.ppm" \
    -o/dev/null 2>&1) && {
    echo "not ok" 1 - "invalid CLI center restarts unexpectedly ignored in favor of env"
    exit 0
}

test "${msg#*restarts must be in range 1-32.*}" != "${msg}" || {
    echo "not ok" 1 - "missing invalid CLI center restarts diagnostic"
    exit 0
}

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env "SIXEL_PALETTE_KCENTER_RESTARTS=0" \
    -Qcenter:restarts=3 \
    "${TOP_SRCDIR}/tests/data/inputs/small.ppm" \
    -o/dev/null >/dev/null 2>&1 || {
    echo "not ok" 1 - "valid CLI center restarts did not override invalid env"
    exit 0
}

echo "ok" 1 - "center restarts follows env/CLI acceptance and CLI priority"
exit 0
