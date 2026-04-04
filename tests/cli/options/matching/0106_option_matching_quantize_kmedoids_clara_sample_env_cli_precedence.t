#!/bin/sh
# TAP test verifying kmedoids clara_sample accepts env/CLI values and keeps CLI priority.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}


echo "1..1"
set -v

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env "SIXEL_PALETTE_KMEDOIDS_CLARA_SAMPLE=0" \
    -Qkmedoids \
    "${TOP_SRCDIR}/tests/data/inputs/small.ppm" \
    -o/dev/null >/dev/null 2>&1 || {
    echo "not ok" 1 - "env-only clara_sample=0 was rejected"
    exit 0
}

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    -Qkmedoids:clara_sample=1048576 \
    "${TOP_SRCDIR}/tests/data/inputs/small.ppm" \
    -o/dev/null >/dev/null 2>&1 || {
    echo "not ok" 1 - "cli-only clara_sample upper bound was rejected"
    exit 0
}

msg=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env "SIXEL_PALETTE_KMEDOIDS_CLARA_SAMPLE=0" \
    -Qkmedoids:clara_sample=63 \
    "${TOP_SRCDIR}/tests/data/inputs/small.ppm" \
    -o/dev/null 2>&1) && {
    echo "not ok" 1 - "invalid CLI clara_sample unexpectedly ignored in favor of env"
    exit 0
}

test "${msg#*-Q clara_sample must be 0 or in range 64-1048576.*}" != "${msg}" || {
    echo "not ok" 1 - "missing invalid CLI clara_sample diagnostic"
    exit 0
}

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env "SIXEL_PALETTE_KMEDOIDS_CLARA_SAMPLE=63" \
    -Qkmedoids:clara_sample=1048576 \
    "${TOP_SRCDIR}/tests/data/inputs/small.ppm" \
    -o/dev/null >/dev/null 2>&1 || {
    echo "not ok" 1 - "valid CLI clara_sample did not override invalid env"
    exit 0
}

echo "ok" 1 - "kmedoids clara_sample follows env/CLI acceptance and CLI priority"
exit 0
