#!/bin/sh
# TAP test: PAL export defaults to JASC layout.

set -eux

. "${TOP_SRCDIR}/tests/lib/sh/common/tap.sh"
. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

ensure_converter_available "IMG2SIXEL" "${IMG2SIXEL_PATH}" "img2sixel"

status=0

echo "1..1"
set -v

snake_png="${TOP_SRCDIR}/tests/data/inputs/snake_64.png"
pal_default="${ARTIFACT_LOCAL_DIR}/palette-default.pal"

if run_img2sixel -M "${pal_default}" -o "${ARTIFACT_LOCAL_DIR}/pal-default.six" \
        "${snake_png}"; then
    if head -n 1 "${pal_default}" | grep -q "JASC-PAL"; then
        pass "PAL export defaults to JASC header"
    else
        fail "PAL export missing JASC header"
    fi
else
    fail "PAL default export failed"
fi

exit "${status}"
