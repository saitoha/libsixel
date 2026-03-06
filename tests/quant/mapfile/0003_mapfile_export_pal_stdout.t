#!/bin/sh
# TAP test: PAL export to stdout retains JASC-PAL header.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

echo "1..1"
set -v
mkdir "${ARTIFACT_LOCAL_DIR}"

snake_png="${TOP_SRCDIR}/tests/data/inputs/snake_64.png"
pal_stdout="${ARTIFACT_LOCAL_DIR}/palette-stdout.pal"

run_img2sixel -M pal:- -o "${ARTIFACT_LOCAL_DIR}/pal-stdout.six"     "${snake_png}" >"${pal_stdout}" || {
    echo "not ok" 1 - "PAL stdout export failed"
    exit 0
}

head -n 1 "${pal_stdout}" | grep -q "JASC-PAL" || {
    echo "not ok" 1 - "PAL stdout export missing JASC-PAL header"
    exit 0
}

echo "ok" 1 - "PAL stdout export emitted JASC-PAL header"

exit 0
