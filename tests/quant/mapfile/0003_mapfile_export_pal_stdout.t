#!/bin/sh
# TAP test: PAL export supports stdout via type prefix.

set -eux

MAPFILE_HELPER_DIR="${TOP_SRCDIR}/tests/lib/sh/mapfile"
. "${MAPFILE_HELPER_DIR}/mapfile_common.sh"

load_mapfile_prereqs

echo "1..1"
set -v

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
