#!/bin/sh
# TAP test: ACT palette input detected by file extension.

set -euxv

script_dir=$(CDPATH=; cd "$(dirname "$0")" && pwd)
. "${script_dir}/mapfile_common.sh"

test_name=$(basename "$0")
setup_mapfile_dirs "${test_name}"
load_mapfile_prereqs "${script_dir}"

echo "1..1"

act_palette="${tmp_dir}/palette.act"
if ! run_img2sixel -M "${act_palette}" -o "${tmp_dir}/act.six" \
        "${snake_png}" 2>>"${log_file}"; then
    fail "Preparing ACT palette for import failed"
    exit "${status}"
fi

if run_img2sixel -m "${act_palette}" -o "${output_dir}/from-act.six" \
        "${snake_png}" 2>>"${log_file}"; then
    if [ -s "${output_dir}/from-act.six" ]; then
        pass "ACT palette input detected by extension"
    else
        fail "ACT palette conversion produced no data"
    fi
else
    fail "ACT palette conversion failed"
fi

exit "${status}"
