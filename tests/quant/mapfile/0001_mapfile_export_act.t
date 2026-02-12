#!/bin/sh
# TAP test: ACT palette export writes expected size.

set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

ensure_converter_available "IMG2SIXEL" "${IMG2SIXEL_PATH}" "img2sixel"

echo "1..1"
set -v

snake_png="${TOP_SRCDIR}/tests/data/inputs/snake_64.png"
act_palette="${ARTIFACT_LOCAL_DIR}/palette.act"

run_img2sixel -M "${act_palette}" -o "${ARTIFACT_LOCAL_DIR}/act.six" "${snake_png}" || {
    fail 1 "ACT palette export failed"
    exit 0
}

act_size=$(wc -c <"${act_palette}")
[ "${act_size}" -eq 772 ] || {
    fail 1 "ACT palette length mismatch (${act_size})"
    exit 0
}

pass 1 "ACT palette exported with correct length"

exit 0
