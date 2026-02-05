#!/bin/sh
# TAP test: PAL export supports stdout via type prefix.

set -eux

script_dir=$(CDPATH=; cd "$(dirname "$0")" && pwd)
MAPFILE_HELPER_DIR="${TOP_SRCDIR}/tests/lib/sh/mapfile"
. "${MAPFILE_HELPER_DIR}/mapfile_common.sh"

setup_mapfile_dirs "${test_name}"
load_mapfile_prereqs "${script_dir}"

echo "1..1"
set -v

pal_stdout="${output_dir}/palette-stdout.pal"
if run_img2sixel -M pal:- -o "${tmp_dir}/pal-stdout.six" \
        "${snake_png}" >"${pal_stdout}"; then
    if head -n 1 "${pal_stdout}" | grep -q "JASC-PAL"; then
        pass "PAL export supports type-prefixed stdout"
    else
        fail "PAL stdout header missing"
    fi
else
    fail "PAL stdout export failed"
fi

exit "${status}"
