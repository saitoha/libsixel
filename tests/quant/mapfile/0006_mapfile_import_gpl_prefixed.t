#!/bin/sh
# TAP test: GPL palette input via type prefix converts image data.

set -eux

MAPFILE_HELPER_DIR="${TOP_SRCDIR}/tests/lib/sh/mapfile"
. "${MAPFILE_HELPER_DIR}/mapfile_common.sh"

load_mapfile_prereqs

echo "1..1"
set -v

gpl_palette="${ARTIFACT_LOCAL_DIR}/palette-gpl.dat"
if ! run_img2sixel -M gpl:"${gpl_palette}" -o "${ARTIFACT_LOCAL_DIR}/pal-gpl.six" \
        "${snake_png}"; then
    fail "Preparing GPL palette for import failed"
    exit "${status}"
fi

if run_img2sixel -m gpl:"${gpl_palette}" \
        -o "${ARTIFACT_LOCAL_DIR}/from-gpl.six" "${snake_png}"; then
    if [ -s "${ARTIFACT_LOCAL_DIR}/from-gpl.six" ]; then
        pass "GPL palette input via type prefix works"
    else
        fail "GPL palette conversion produced no data"
    fi
else
    fail "GPL palette conversion failed"
fi

exit "${status}"
