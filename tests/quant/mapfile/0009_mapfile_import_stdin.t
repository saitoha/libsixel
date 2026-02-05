#!/bin/sh
# TAP test: palette input accepted from stdin.

set -eux

. "${TOP_SRCDIR}/tests/lib/sh/common/tap.sh"
. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

ensure_converter_available "IMG2SIXEL" "${IMG2SIXEL_PATH}" "img2sixel"

status=0

echo "1..1"
set -v

snake_png="${TOP_SRCDIR}/tests/data/inputs/snake_64.png"
gpl_palette="${ARTIFACT_LOCAL_DIR}/palette-gpl.dat"

if ! run_img2sixel -M gpl:"${gpl_palette}" -o "${ARTIFACT_LOCAL_DIR}/pal-gpl.six" \
        "${snake_png}"; then
    fail "Preparing GPL palette for stdin import failed"
    exit "${status}"
fi

if cat "${gpl_palette}" | run_img2sixel -m gpl:- \
        -o "${ARTIFACT_LOCAL_DIR}/from-stdin.six" "${snake_png}"; then
    if [ -s "${ARTIFACT_LOCAL_DIR}/from-stdin.six" ]; then
        pass "Palette input accepted from stdin"
    else
        fail "stdin palette conversion produced no data"
    fi
else
    fail "stdin palette conversion failed"
fi

exit "${status}"
