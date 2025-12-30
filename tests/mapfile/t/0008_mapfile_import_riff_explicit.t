#!/bin/sh
# TAP test: RIFF palette parsed with explicit type prefix despite missing extension.

set -euxv

script_dir=$(CDPATH=; cd "$(dirname "$0")" && pwd)
. "${script_dir}/mapfile_common.sh"

test_name=$(basename "$0")
setup_mapfile_dirs "${test_name}"
load_mapfile_prereqs "${script_dir}"

echo "1..1"

riff_palette="${tmp_dir}/palette-riff.pal"
if ! run_img2sixel -M pal-riff:"${riff_palette}" \
        -o "${tmp_dir}/pal-riff.six" "${snake_png}" 2>>"${log_file}"; then
    fail "Preparing RIFF palette for import failed"
    exit "${status}"
fi

riff_alias="${tmp_dir}/palette-riff-noext"
cp "${riff_palette}" "${riff_alias}"
if run_img2sixel -m pal-riff:"${riff_alias}" \
        -o "${output_dir}/from-riff.six" "${snake_png}" 2>>"${log_file}"; then
    if [ -s "${output_dir}/from-riff.six" ]; then
        pass "RIFF palette parsed with explicit type"
    else
        fail "RIFF palette conversion produced no data"
    fi
else
    fail "RIFF palette conversion failed"
fi

exit "${status}"
