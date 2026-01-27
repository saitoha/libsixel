#!/bin/sh
# TAP test: PAL export defaults to JASC layout.

set -eux

script_dir=$(CDPATH=; cd "$(dirname "$0")" && pwd)
MAPFILE_HELPER_DIR="${script_dir}/../../lib/sh/mapfile"
. "${MAPFILE_HELPER_DIR}/mapfile_common.sh"

test_name=$(basename "$0")
setup_mapfile_dirs "${test_name}"
load_mapfile_prereqs "${script_dir}"

echo "1..1"
set -v

pal_default="${tmp_dir}/palette-default.pal"
if run_img2sixel -M "${pal_default}" -o "${tmp_dir}/pal-default.six" \
        "${snake_png}" 2>>"${log_file}"; then
    if head -n 1 "${pal_default}" | grep -q "JASC-PAL"; then
        pass "PAL export defaults to JASC header"
    else
        fail "PAL export missing JASC header"
    fi
else
    fail "PAL default export failed"
fi

exit "${status}"
