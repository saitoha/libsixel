#!/bin/sh
# TAP test: ACT palette input detected by file extension.

set -eux

MAPFILE_HELPER_DIR="${TOP_SRCDIR}/tests/lib/sh/mapfile"
. "${MAPFILE_HELPER_DIR}/mapfile_common.sh"

load_mapfile_prereqs

echo "1..1"
set -v

act_palette="${ARTIFACT_LOCAL_DIR}/palette.act"
if ! run_img2sixel -M "${act_palette}" -o "${ARTIFACT_LOCAL_DIR}/act.six" \
        "${snake_png}"; then
    fail "Preparing ACT palette for import failed"
    exit "${status}"
fi

if run_img2sixel -m "${act_palette}" -o "${ARTIFACT_LOCAL_DIR}/from-act.six" \
        "${snake_png}"; then
    if [ -s "${ARTIFACT_LOCAL_DIR}/from-act.six" ]; then
        pass "ACT palette input detected by extension"
    else
        fail "ACT palette conversion produced no data"
    fi
else
    fail "ACT palette conversion failed"
fi

exit "${status}"
