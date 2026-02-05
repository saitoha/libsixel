#!/bin/sh
# TAP test: ACT palette export writes expected size.

set -eux

. "${TOP_SRCDIR}/tests/lib/sh/common/tap.sh"
. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

ensure_converter_available "IMG2SIXEL" "${IMG2SIXEL_PATH}" "img2sixel"

status=0

echo "1..1"
set -v

snake_png="${TOP_SRCDIR}/tests/data/inputs/snake_64.png"
act_palette="${ARTIFACT_LOCAL_DIR}/palette.act"

if run_img2sixel -M "${act_palette}" -o "${ARTIFACT_LOCAL_DIR}/act.six" "${snake_png}"; then
    act_size=$(wc -c <"${act_palette}")
    if [ "${act_size}" -eq 772 ]; then
        pass "ACT palette exported with correct length"
    else
        fail "ACT palette length mismatch (${act_size})"
    fi
else
    fail "ACT palette export failed"
fi

exit "${status}"
