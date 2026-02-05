#!/bin/sh
# TAP test: RIFF palette parsed with explicit type prefix despite missing extension.

set -eux

. "${TOP_SRCDIR}/tests/lib/sh/common/tap.sh"
. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

ensure_converter_available "IMG2SIXEL" "${IMG2SIXEL_PATH}" "img2sixel"

status=0

echo "1..1"
set -v

snake_png="${TOP_SRCDIR}/tests/data/inputs/snake_64.png"
riff_palette="${ARTIFACT_LOCAL_DIR}/palette-riff.pal"

if ! run_img2sixel -M pal-riff:"${riff_palette}" \
        -o "${ARTIFACT_LOCAL_DIR}/pal-riff.six" "${snake_png}"; then
    fail "Preparing RIFF palette for import failed"
    exit "${status}"
fi

riff_alias="${ARTIFACT_LOCAL_DIR}/palette-riff-noext"
cp "${riff_palette}" "${riff_alias}"
if run_img2sixel -m pal-riff:"${riff_alias}" \
        -o "${ARTIFACT_LOCAL_DIR}/from-riff.six" "${snake_png}"; then
    if [ -s "${ARTIFACT_LOCAL_DIR}/from-riff.six" ]; then
        pass "RIFF palette parsed with explicit type"
    else
        fail "RIFF palette conversion produced no data"
    fi
else
    fail "RIFF palette conversion failed"
fi

exit "${status}"
