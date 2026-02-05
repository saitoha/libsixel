#!/bin/sh
# TAP test: ACT palette export writes expected size.

set -eux

script_dir=$(CDPATH=; cd "${0%[/\\]*}" && pwd)
MAPFILE_HELPER_DIR="${TOP_SRCDIR}/tests/lib/sh/mapfile"
. "${MAPFILE_HELPER_DIR}/mapfile_common.sh"

setup_mapfile_dirs "${test_name}"
load_mapfile_prereqs "${script_dir}"

echo "1..1"
set -v

act_palette="${tmp_dir}/palette.act"
if run_img2sixel -M "${act_palette}" -o "${tmp_dir}/act.six" \
        "${snake_png}"; then
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
