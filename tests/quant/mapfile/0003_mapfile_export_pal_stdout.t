#!/bin/sh
# TAP test: PAL export to stdout retains JASC-PAL header.

set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

ensure_converter_available "IMG2SIXEL" "${IMG2SIXEL_PATH}" "img2sixel"

echo "1..1"
set -v

snake_png="${TOP_SRCDIR}/tests/data/inputs/snake_64.png"
pal_stdout="${ARTIFACT_LOCAL_DIR}/palette-stdout.pal"

run_img2sixel -M pal:- -o "${ARTIFACT_LOCAL_DIR}/pal-stdout.six"     "${snake_png}" >"${pal_stdout}" || {
    fail 1 "PAL stdout export failed"
    exit 0
}

head -n 1 "${pal_stdout}" | grep -q "JASC-PAL" || {
    fail 1 "PAL stdout export missing JASC-PAL header"
    exit 0
}

pass 1 "PAL stdout export emitted JASC-PAL header"

exit 0
