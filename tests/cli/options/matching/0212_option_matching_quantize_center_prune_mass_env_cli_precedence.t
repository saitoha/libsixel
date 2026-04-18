#!/bin/sh
# TAP test verifying center prune_mass accepts env/CLI values and keeps CLI priority.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}


echo "1..1"
set -v

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env "SIXEL_PALETTE_KCENTER_PRUNE_MASS=0.900" \
    -Qcenter \
    "${TOP_SRCDIR}/tests/data/inputs/small.ppm" \
    -o/dev/null >/dev/null 2>&1 || {
    echo "not ok" 1 - "env-only center prune_mass=0.900 was rejected"
    exit 0
}

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    -Qcenter:prune_mass=1.000 \
    "${TOP_SRCDIR}/tests/data/inputs/small.ppm" \
    -o/dev/null >/dev/null 2>&1 || {
    echo "not ok" 1 - "cli-only center prune_mass upper bound was rejected"
    exit 0
}

msg=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env "SIXEL_PALETTE_KCENTER_PRUNE_MASS=0.900" \
    -Qcenter:prune_mass=0.899 \
    "${TOP_SRCDIR}/tests/data/inputs/small.ppm" \
    -o/dev/null 2>&1) && {
    echo "not ok" 1 - "invalid CLI center prune_mass unexpectedly ignored in favor of env"
    exit 0
}

test "${msg#*prune_mass must be in range 0.900-1.000.*}" != "${msg}" || {
    echo "not ok" 1 - "missing invalid CLI center prune_mass diagnostic"
    exit 0
}

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env "SIXEL_PALETTE_KCENTER_PRUNE_MASS=0.899" \
    -Qcenter:prune_mass=1.000 \
    "${TOP_SRCDIR}/tests/data/inputs/small.ppm" \
    -o/dev/null >/dev/null 2>&1 || {
    echo "not ok" 1 - "valid CLI center prune_mass did not override invalid env"
    exit 0
}

echo "ok" 1 - "center prune_mass follows env/CLI acceptance and CLI priority"
exit 0
