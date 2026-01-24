#!/bin/sh
# TAP test: RIFF palette export honours type prefix.

set -euxv

script_dir=$(CDPATH=; cd "$(dirname "$0")" && pwd)
MAPFILE_HELPER_DIR="${script_dir}/../../lib/sh/mapfile"
. "${MAPFILE_HELPER_DIR}/mapfile_common.sh"

test_name=$(basename "$0")
setup_mapfile_dirs "${test_name}"
load_mapfile_prereqs "${script_dir}"

echo "1..1"

riff_palette="${tmp_dir}/palette-riff.pal"
if run_img2sixel -M pal-riff:"${riff_palette}" \
        -o "${tmp_dir}/pal-riff.six" "${snake_png}" 2>>"${log_file}"; then
    riff_header=$(dd if="${riff_palette}" bs=1 count=4 2>/dev/null |
        LC_ALL=C od -An -tx1 | tr -d ' \n')
    if [ "${riff_header}" = "52494646" ]; then
        pass "RIFF palette export honours type prefix"
    else
        fail "RIFF palette header incorrect (${riff_header})"
    fi
else
    fail "RIFF palette export failed"
fi

exit "${status}"
