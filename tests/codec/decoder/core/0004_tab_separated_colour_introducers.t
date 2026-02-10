#!/bin/sh
# TAP test checking tab-separated colour introducers are decoded successfully.

set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

ensure_converter_available "IMG2SIXEL" "${IMG2SIXEL_PATH}" "img2sixel"

echo "1..1"
set -v

snake_png="${TOP_SRCDIR}/tests/data/inputs/snake_64.png"

if run_img2sixel "${snake_png}" >"${ARTIFACT_LOCAL_DIR}/case01-stage.sixel" && \
        sed 's/C/C:/g' "${ARTIFACT_LOCAL_DIR}/case01-stage.sixel" | tr ':' '\t' | \
        run_img2sixel >"${ARTIFACT_LOCAL_DIR}/case01.sixel"; then
    pass 1 "tab-separated colour introducers handled"
else
    fail 1 "tab-separated colour introducers rejected"
fi

exit 0
