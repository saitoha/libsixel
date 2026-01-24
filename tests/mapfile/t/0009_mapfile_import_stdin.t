#!/bin/sh
# TAP test: palette input accepted from stdin.

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
    fail "Preparing GPL palette for stdin import failed"
    exit "${status}"
fi

if cat "${gpl_palette}" | run_img2sixel -m gpl:- \
        -o "${output_dir}/from-stdin.six" "${snake_png}" 2>>"${log_file}"; then
    if [ -s "${output_dir}/from-stdin.six" ]; then
        pass "Palette input accepted from stdin"
    else
        fail "stdin palette conversion produced no data"
    fi
else
    fail "stdin palette conversion failed"
fi

exit "${status}"
