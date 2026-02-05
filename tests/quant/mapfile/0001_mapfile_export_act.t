#!/bin/sh
# TAP test: ACT palette export writes expected size.

set -eux

MAPFILE_HELPER_DIR="${TOP_SRCDIR}/tests/lib/sh/mapfile"
. "${MAPFILE_HELPER_DIR}/mapfile_common.sh"

load_mapfile_prereqs

echo "1..1"
set -v

act_palette="${ARTIFACT_LOCAL_DIR}/palette.act"
if run_img2sixel -M "${act_palette}" -o "${ARTIFACT_LOCAL_DIR}/act.six" "${snake_png}"; then
    act_size=$(wc -c <"${act_palette}")
    if [ "${act_size}" -eq 772 ]; then
        pass "ACT palette exported with correct length"
    else
        fail "ACT palette length mismatch (${act_size})"
    fi
else
    fail "ACT palette export failed"
fi

exit "${status}"
