#!/bin/sh
# TAP test verifying center budget_scale keeps CLI priority over env.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}


echo "1..1"
set -v

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env "SIXEL_PALETTE_KCENTER_BUDGET_SCALE=1.40" \
    -Qcenter \
    "${TOP_SRCDIR}/tests/data/inputs/small.ppm" \
    -o/dev/null >/dev/null 2>&1 || {
    echo "not ok" 1 - "env-only center budget_scale=1.40 was rejected"
    exit 0
}

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    -Qcenter:budget_scale=0.80 \
    "${TOP_SRCDIR}/tests/data/inputs/small.ppm" \
    -o/dev/null >/dev/null 2>&1 || {
    echo "not ok" 1 - "cli-only center budget_scale=0.80 was rejected"
    exit 0
}

msg=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env "SIXEL_PALETTE_KCENTER_BUDGET_SCALE=1.40" \
    -Qcenter:budget_scale=4.20 \
    "${TOP_SRCDIR}/tests/data/inputs/small.ppm" \
    -o/dev/null 2>&1) && {
    echo "not ok" 1 - "invalid CLI budget_scale unexpectedly ignored in favor of env"
    exit 0
}

test "${msg#*budget_scale*}" != "${msg}" || {
    echo "not ok" 1 - "missing invalid CLI center budget_scale diagnostic"
    exit 0
}

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env "SIXEL_PALETTE_KCENTER_BUDGET_SCALE=4.20" \
    -Qcenter:budget_scale=1.10 \
    "${TOP_SRCDIR}/tests/data/inputs/small.ppm" \
    -o/dev/null >/dev/null 2>&1 || {
    echo "not ok" 1 - "valid CLI center budget_scale did not override invalid env"
    exit 0
}

echo "ok" 1 - "center budget_scale follows env/CLI acceptance and CLI priority"
exit 0
