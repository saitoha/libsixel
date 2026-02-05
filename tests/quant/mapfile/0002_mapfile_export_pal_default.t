#!/bin/sh
# TAP test: PAL export defaults to JASC layout.

set -eux

script_dir=$(CDPATH=; cd "${0%[/\\]*}" && pwd)
MAPFILE_HELPER_DIR="${TOP_SRCDIR}/tests/lib/sh/mapfile"
. "${MAPFILE_HELPER_DIR}/mapfile_common.sh"

setup_mapfile_dirs "${test_name}"
load_mapfile_prereqs "${script_dir}"

echo "1..1"
set -v

pal_default="${ARTIFACT_LOCAL_DIR}/palette-default.pal"
if run_img2sixel -M "${pal_default}" -o "${ARTIFACT_LOCAL_DIR}/pal-default.six" \
        "${snake_png}"; then
    if head -n 1 "${pal_default}" | grep -q "JASC-PAL"; then
        pass "PAL export defaults to JASC header"
    else
        fail "PAL export missing JASC header"
    fi
else
    fail "PAL default export failed"
fi

exit "${status}"
