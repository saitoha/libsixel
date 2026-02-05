#!/bin/sh
# TAP test: RIFF palette parsed with explicit type prefix despite missing extension.

set -eux

script_dir=$(CDPATH=; cd "${0%[/\\]*}" && pwd)
MAPFILE_HELPER_DIR="${TOP_SRCDIR}/tests/lib/sh/mapfile"
. "${MAPFILE_HELPER_DIR}/mapfile_common.sh"

setup_mapfile_dirs "${test_name}"
load_mapfile_prereqs "${script_dir}"

echo "1..1"
set -v

riff_palette="${tmp_dir}/palette-riff.pal"
if ! run_img2sixel -M pal-riff:"${riff_palette}" \
        -o "${tmp_dir}/pal-riff.six" "${snake_png}"; then
    fail "Preparing RIFF palette for import failed"
    exit "${status}"
fi

riff_alias="${tmp_dir}/palette-riff-noext"
cp "${riff_palette}" "${riff_alias}"
if run_img2sixel -m pal-riff:"${riff_alias}" \
        -o "${output_dir}/from-riff.six" "${snake_png}"; then
    if [ -s "${output_dir}/from-riff.six" ]; then
        pass "RIFF palette parsed with explicit type"
    else
        fail "RIFF palette conversion produced no data"
    fi
else
    fail "RIFF palette conversion failed"
fi

exit "${status}"
