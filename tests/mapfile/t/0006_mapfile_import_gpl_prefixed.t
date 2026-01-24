#!/bin/sh
# TAP test: GPL palette input via type prefix converts image data.

set -euxv

script_dir=$(CDPATH=; cd "$(dirname "$0")" && pwd)
MAPFILE_HELPER_DIR="${script_dir}/../../lib/sh/mapfile"
. "${MAPFILE_HELPER_DIR}/mapfile_common.sh"

test_name=$(basename "$0")
setup_mapfile_dirs "${test_name}"
load_mapfile_prereqs "${script_dir}"

echo "1..1"

gpl_palette="${tmp_dir}/palette-gpl.dat"
if ! run_img2sixel -M gpl:"${gpl_palette}" -o "${tmp_dir}/pal-gpl.six" \
        "${snake_png}" 2>>"${log_file}"; then
    fail "Preparing GPL palette for import failed"
    exit "${status}"
fi

if run_img2sixel -m gpl:"${gpl_palette}" \
        -o "${output_dir}/from-gpl.six" "${snake_png}" 2>>"${log_file}"; then
    if [ -s "${output_dir}/from-gpl.six" ]; then
        pass "GPL palette input via type prefix works"
    else
        fail "GPL palette conversion produced no data"
    fi
else
    fail "GPL palette conversion failed"
fi

exit "${status}"
