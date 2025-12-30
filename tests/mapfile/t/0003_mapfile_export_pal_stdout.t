#!/bin/sh
# TAP test: PAL export supports stdout via type prefix.

set -euxv

script_dir=$(CDPATH=; cd "$(dirname "$0")" && pwd)
. "${script_dir}/mapfile_common.sh"

test_name=$(basename "$0")
setup_mapfile_dirs "${test_name}"
load_mapfile_prereqs "${script_dir}"

echo "1..1"

pal_stdout="${output_dir}/palette-stdout.pal"
if run_img2sixel -M pal:- -o "${tmp_dir}/pal-stdout.six" \
        "${snake_png}" >"${pal_stdout}" 2>>"${log_file}"; then
    if head -n 1 "${pal_stdout}" | grep -q "JASC-PAL"; then
        pass "PAL export supports type-prefixed stdout"
    else
        fail "PAL stdout header missing"
    fi
else
    fail "PAL stdout export failed"
fi

exit "${status}"
