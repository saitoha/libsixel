#!/bin/sh
# TAP test: PAL export defaults to JASC layout.

set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

ensure_converter_available "IMG2SIXEL" "${IMG2SIXEL_PATH}" "img2sixel"

echo "1..1"
set -v

snake_png="${TOP_SRCDIR}/tests/data/inputs/snake_64.png"
pal_default="${ARTIFACT_LOCAL_DIR}/palette-default.pal"

if run_img2sixel -M "${pal_default}" -o "${ARTIFACT_LOCAL_DIR}/pal-default.six" \
        "${snake_png}"; then
    if head -n 1 "${pal_default}" | grep -q "JASC-PAL"; then
        pass 1 "PAL export defaults to JASC header"
    else
        fail 1 "PAL export missing JASC header"
    fi
else
    fail 1 "PAL default export failed"
fi

exit 0
