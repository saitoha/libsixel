#!/bin/sh
# TAP test: ACT palette input detected by file extension.

set -eux

. "${TOP_SRCDIR}/tests/lib/sh/common/tap.sh"
. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

ensure_converter_available "IMG2SIXEL" "${IMG2SIXEL_PATH}" "img2sixel"

status=0

echo "1..1"
set -v

snake_png="${TOP_SRCDIR}/tests/data/inputs/snake_64.png"
act_palette="${ARTIFACT_LOCAL_DIR}/palette.act"

if ! run_img2sixel -M "${act_palette}" -o "${ARTIFACT_LOCAL_DIR}/act.six" \
        "${snake_png}"; then
    fail "Preparing ACT palette for import failed"
    exit "${status}"
fi

if run_img2sixel -m "${act_palette}" -o "${ARTIFACT_LOCAL_DIR}/from-act.six" \
        "${snake_png}"; then
    if [ -s "${ARTIFACT_LOCAL_DIR}/from-act.six" ]; then
        pass "ACT palette input detected by extension"
    else
        fail "ACT palette conversion produced no data"
    fi
else
    fail "ACT palette conversion failed"
fi

exit "${status}"
