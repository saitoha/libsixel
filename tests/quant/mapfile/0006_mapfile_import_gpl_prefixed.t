#!/bin/sh
# TAP test: GPL palette input via type prefix converts image data.

set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

ensure_converter_available "IMG2SIXEL" "${IMG2SIXEL_PATH}" "img2sixel"

echo "1..1"
set -v

snake_png="${TOP_SRCDIR}/tests/data/inputs/snake_64.png"
gpl_palette="${ARTIFACT_LOCAL_DIR}/palette-gpl.dat"

if ! run_img2sixel -M gpl:"${gpl_palette}" -o "${ARTIFACT_LOCAL_DIR}/pal-gpl.six" \
        "${snake_png}"; then
    fail "Preparing GPL palette for import failed"
    exit "${status}"
fi

if run_img2sixel -m gpl:"${gpl_palette}" \
        -o "${ARTIFACT_LOCAL_DIR}/from-gpl.six" "${snake_png}"; then
    if [ -s "${ARTIFACT_LOCAL_DIR}/from-gpl.six" ]; then
        pass 1 "GPL palette input via type prefix works"
    else
        fail 1 "GPL palette conversion produced no data"
    fi
else
    fail 1 "GPL palette conversion failed"
fi

exit 0
