#!/bin/sh
# TAP test: RIFF palette export honours type prefix.

set -eux

MAPFILE_HELPER_DIR="${TOP_SRCDIR}/tests/lib/sh/mapfile"
. "${MAPFILE_HELPER_DIR}/mapfile_common.sh"

load_mapfile_prereqs

echo "1..1"
set -v

riff_palette="${ARTIFACT_LOCAL_DIR}/palette-riff.pal"
if run_img2sixel -M pal-riff:"${riff_palette}" \
        -o "${ARTIFACT_LOCAL_DIR}/pal-riff.six" "${snake_png}"; then
    riff_header=$(dd if="${riff_palette}" bs=1 count=4 2>/dev/null |
        LC_ALL=C od -An -tx1 | tr -d ' \n')
    if [ "${riff_header}" = "52494646" ]; then
        pass "RIFF palette export honours type prefix"
    else
        fail "RIFF palette header incorrect (${riff_header})"
    fi
else
    fail "RIFF palette export failed"
fi

exit "${status}"
