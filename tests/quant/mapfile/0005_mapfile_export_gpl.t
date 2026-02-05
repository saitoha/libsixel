#!/bin/sh
# TAP test: GPL palette export writes expected header.

set -eux

MAPFILE_HELPER_DIR="${TOP_SRCDIR}/tests/lib/sh/mapfile"
. "${MAPFILE_HELPER_DIR}/mapfile_common.sh"

load_mapfile_prereqs

echo "1..1"
set -v

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
