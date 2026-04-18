#!/bin/sh
# TAP test verifying center histbits accepts env/CLI values and keeps CLI priority.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}


echo "1..1"
set -v

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env "SIXEL_PALETTE_KCENTER_HISTBITS=5" \
    -Qcenter \
    "${TOP_SRCDIR}/tests/data/inputs/small.ppm" \
    -o/dev/null >/dev/null 2>&1 || {
    echo "not ok" 1 - "env-only center histbits=5 was rejected"
    exit 0
}

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    -Qcenter:histbits=6 \
    "${TOP_SRCDIR}/tests/data/inputs/small.ppm" \
    -o/dev/null >/dev/null 2>&1 || {
    echo "not ok" 1 - "cli-only center histbits upper bound was rejected"
    exit 0
}

msg=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env "SIXEL_PALETTE_KCENTER_HISTBITS=5" \
    -Qcenter:histbits=7 \
    "${TOP_SRCDIR}/tests/data/inputs/small.ppm" \
    -o/dev/null 2>&1) && {
    echo "not ok" 1 - "invalid CLI center histbits unexpectedly ignored in favor of env"
    exit 0
}

test "${msg#*histbits must be in range 3-6.*}" != "${msg}" || {
    echo "not ok" 1 - "missing invalid CLI center histbits diagnostic"
    exit 0
}

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env "SIXEL_PALETTE_KCENTER_HISTBITS=7" \
    -Qcenter:histbits=4 \
    "${TOP_SRCDIR}/tests/data/inputs/small.ppm" \
    -o/dev/null >/dev/null 2>&1 || {
    echo "not ok" 1 - "valid CLI center histbits did not override invalid env"
    exit 0
}

echo "ok" 1 - "center histbits follows env/CLI acceptance and CLI priority"
exit 0
