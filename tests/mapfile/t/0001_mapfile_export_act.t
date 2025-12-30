#!/bin/sh
# TAP test: ACT palette export writes expected size.

set -euxv

script_dir=$(CDPATH=; cd "$(dirname "$0")" && pwd)
. "${script_dir}/mapfile_common.sh"

test_name=$(basename "$0")
setup_mapfile_dirs "${test_name}"
load_mapfile_prereqs "${script_dir}"

echo "1..1"

act_palette="${tmp_dir}/palette.act"
if run_img2sixel -M "${act_palette}" -o "${tmp_dir}/act.six" \
        "${snake_png}" 2>>"${log_file}"; then
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
