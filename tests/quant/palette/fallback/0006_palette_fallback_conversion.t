#!/bin/sh
# TAP test: PAL1 input expands via fallback path when tables are disabled.

set -eux

test_name=$(basename "$0")
test_dir=$(CDPATH=; cd "$(dirname "$0")" && pwd)
category_name=$(basename "$(dirname "${test_dir}")")
artifact_root=${ARTIFACT_ROOT:-"$(pwd)/_artifacts"}
artifact_test_dir=$(dirname "$0")
artifact_dir="${artifact_root}/${artifact_test_dir}/${test_name}"
log_file="${artifact_dir}/palette-fallback.log"
output_dir="${artifact_dir}/outputs"

tmp_dir="${artifact_dir}/tmp"

mkdir -p "${output_dir}" "${tmp_dir}"

script_dir=$(CDPATH=; cd "$(dirname "$0")" && pwd)
. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

ensure_converter_available "IMG2SIXEL" "${IMG2SIXEL_PATH}" "img2sixel"

echo "1..1"
set -v

snake_ascii_pbm="${images_dir}/snake-ascii.pbm"
require_file "${snake_ascii_pbm}"

if SIXEL_PALETTE_DISABLE_TABLES=1 \
        run_img2sixel "${snake_ascii_pbm}" \
        -o "${output_dir}/snake-fallback.sixel" \
        >"${log_file}" 2>&1; then
    printf 'ok 1 - PAL1 input expands via fallback path\n'
else
    printf 'not ok 1 - fallback expansion failed\n'
    exit 1
fi
