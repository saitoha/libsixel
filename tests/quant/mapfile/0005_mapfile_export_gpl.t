#!/bin/sh
# TAP test: GPL palette export writes expected header.

set -eux

. "${TOP_SRCDIR}/tests/lib/sh/common/tap.sh"
. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

ensure_converter_available "IMG2SIXEL" "${IMG2SIXEL_PATH}" "img2sixel"

status=0

echo "1..1"
set -v

snake_png="${TOP_SRCDIR}/tests/data/inputs/snake_64.png"
gpl_palette="${ARTIFACT_LOCAL_DIR}/palette-gpl.dat"

if run_img2sixel -M gpl:"${gpl_palette}" -o "${ARTIFACT_LOCAL_DIR}/pal-gpl.six" "${snake_png}"; then
    if head -n 1 "${gpl_palette}" | grep -q "GIMP Palette"; then
        pass "GPL palette export writes header"
    else
        fail "GPL palette header missing"
    fi
else
    fail "GPL palette export failed"
fi

exit "${status}"
