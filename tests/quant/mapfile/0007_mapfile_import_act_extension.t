#!/bin/sh
# TAP test: ACT palette import by extension converts image data.

set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

ensure_converter_available "IMG2SIXEL" "${IMG2SIXEL_PATH}" "img2sixel"

echo "1..1"
set -v

snake_png="${TOP_SRCDIR}/tests/data/inputs/snake_64.png"
act_palette="${ARTIFACT_LOCAL_DIR}/palette.act"

run_img2sixel -M "${act_palette}" -o "${ARTIFACT_LOCAL_DIR}/act.six"         "${snake_png}" || {
    fail 1 "Preparing ACT palette for import failed"
    exit 0
}

run_img2sixel -m "${act_palette}" -o "${ARTIFACT_LOCAL_DIR}/from-act.six"         "${snake_png}" || {
    fail 1 "ACT palette conversion failed"
    exit 0
}

[ -s "${ARTIFACT_LOCAL_DIR}/from-act.six" ] || {
    fail 1 "ACT palette conversion produced no data"
    exit 0
}

pass 1 "ACT palette import by extension works"

exit 0
