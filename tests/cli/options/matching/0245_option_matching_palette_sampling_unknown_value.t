#!/bin/sh
# Verify palette-sampling rejects an unknown policy name.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v

status=0
message=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --palette-sampling=unknown \
    "${TOP_SRCDIR}/tests/data/inputs/small.ppm" \
    -o/dev/null 2>&1) || status=$?

test "${status}" -eq 2 || {
    echo "not ok 1 - unknown palette-sampling policy exit status mismatch"
    exit 0
}

test "${message#*--palette-sampling*}" != "${message}" || {
    echo "not ok 1 - unknown palette-sampling diagnostic is missing"
    exit 0
}

echo "ok 1 - palette-sampling rejects an unknown policy"
exit 0
