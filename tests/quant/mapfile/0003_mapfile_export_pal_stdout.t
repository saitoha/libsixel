#!/bin/sh
# TAP test: PAL export supports stdout via type prefix.

set -eux

. "${TOP_SRCDIR}/tests/lib/sh/common/tap.sh"
. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

ensure_converter_available "IMG2SIXEL" "${IMG2SIXEL_PATH}" "img2sixel"

status=0

echo "1..1"
set -v

snake_png="${TOP_SRCDIR}/tests/data/inputs/snake_64.png"
pal_stdout="${ARTIFACT_LOCAL_DIR}/palette-stdout.pal"

if run_img2sixel -M pal:- -o "${ARTIFACT_LOCAL_DIR}/pal-stdout.six" \
        "${snake_png}" >"${pal_stdout}"; then
    if head -n 1 "${pal_stdout}" | grep -q "JASC-PAL"; then
        pass "PAL export supports type-prefixed stdout"
    else
        fail "PAL stdout header missing"
    fi
else
    fail "PAL stdout export failed"
fi

exit "${status}"
