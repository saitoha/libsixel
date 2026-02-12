#!/bin/sh
# TAP test: RIFF palette export writes RIFF header bytes.

set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

ensure_converter_available "IMG2SIXEL" "${IMG2SIXEL_PATH}" "img2sixel"

echo "1..1"
set -v

snake_png="${TOP_SRCDIR}/tests/data/inputs/snake_64.png"
riff_palette="${ARTIFACT_LOCAL_DIR}/palette-riff.pal"

run_img2sixel -M pal-riff:"${riff_palette}"     -o "${ARTIFACT_LOCAL_DIR}/pal-riff.six" "${snake_png}" || {
    fail 1 "RIFF palette export failed"
    exit 0
}

riff_header=$(od -An -tx1 -N4 "${riff_palette}" | tr -d ' 
')
[ "${riff_header}" = "52494646" ] || {
    fail 1 "RIFF palette export missing RIFF header"
    exit 0
}

pass 1 "RIFF palette export has RIFF header"

exit 0
