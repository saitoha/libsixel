#!/bin/sh
# TAP test: ACT palette input detected by file extension.

set -eux

script_dir=$(CDPATH=; cd "${0%[/\\]*}" && pwd)
MAPFILE_HELPER_DIR="${TOP_SRCDIR}/tests/lib/sh/mapfile"
. "${MAPFILE_HELPER_DIR}/mapfile_common.sh"

setup_mapfile_dirs "${test_name}"
load_mapfile_prereqs "${script_dir}"

echo "1..1"
set -v

act_palette="${tmp_dir}/palette.act"
if ! run_img2sixel -M "${act_palette}" -o "${tmp_dir}/act.six" \
        "${snake_png}"; then
    fail "Preparing ACT palette for import failed"
    exit "${status}"
fi

if run_img2sixel -m "${act_palette}" -o "${output_dir}/from-act.six" \
        "${snake_png}"; then
    if [ -s "${output_dir}/from-act.six" ]; then
        pass "ACT palette input detected by extension"
    else
        fail "ACT palette conversion produced no data"
    fi
else
    fail "ACT palette conversion failed"
fi

exit "${status}"
