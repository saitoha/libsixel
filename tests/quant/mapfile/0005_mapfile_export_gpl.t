#!/bin/sh
# TAP test: GPL palette export writes expected header.

set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

ensure_converter_available "IMG2SIXEL" "${IMG2SIXEL_PATH}" "img2sixel"

echo "1..1"
set -v

snake_png="${TOP_SRCDIR}/tests/data/inputs/snake_64.png"
gpl_palette="${ARTIFACT_LOCAL_DIR}/palette-gpl.dat"

if run_img2sixel -M gpl:"${gpl_palette}" -o "${ARTIFACT_LOCAL_DIR}/pal-gpl.six" "${snake_png}"; then
    if head -n 1 "${gpl_palette}" | grep -q "GIMP Palette"; then
        pass 1 "GPL palette export writes header"
    else
        fail 1 "GPL palette header missing"
    fi
else
    fail 1 "GPL palette export failed"
fi

exit 0
