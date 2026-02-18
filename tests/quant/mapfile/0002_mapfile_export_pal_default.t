#!/bin/sh
# TAP test: default PAL palette export writes JASC-PAL header.

set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

test "${HAVE_IMG2SIXEL-}" = 1 || skip_all "img2sixel is disabled in this build"

echo "1..1"
set -v

snake_png="${TOP_SRCDIR}/tests/data/inputs/snake_64.png"
pal_default="${ARTIFACT_LOCAL_DIR}/palette-default.pal"

run_img2sixel -M "${pal_default}" -o "${ARTIFACT_LOCAL_DIR}/pal-default.six"     "${snake_png}" || {
    fail 1 "PAL palette export failed"
    exit 0
}

head -n 1 "${pal_default}" | grep -q "JASC-PAL" || {
    fail 1 "PAL palette missing JASC-PAL header"
    exit 0
}

pass 1 "PAL palette exported with JASC-PAL header"

exit 0
