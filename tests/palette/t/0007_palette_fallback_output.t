#!/bin/sh
# TAP test: fallback path produces output data when tables are disabled.

set -euxv

test_name=$(basename "$0")
test_dir=$(CDPATH=; cd "$(dirname "$0")" && pwd)
category_name=$(basename "$(dirname "${test_dir}")")
artifact_root=${ARTIFACT_ROOT:-"$(pwd)/_artifacts"}
artifact_dir="${artifact_root}/${category_name}/${test_name}"
log_file="${artifact_dir}/palette-fallback.log"
output_dir="${artifact_dir}/outputs"

tmp_dir="${artifact_dir}/tmp"

mkdir -p "${output_dir}" "${tmp_dir}"

script_dir=$(CDPATH=; cd "$(dirname "$0")" && pwd)
. "${script_dir}/../../common/t/0001_converters_common.t"

ensure_converter_available "IMG2SIXEL" "${IMG2SIXEL_PATH}" "img2sixel"

echo "1..1"

snake_ascii_pbm="${images_dir}/snake-ascii.pbm"
require_file "${snake_ascii_pbm}"

SIXEL_PALETTE_DISABLE_TABLES=1 \
    run_img2sixel "${snake_ascii_pbm}" \
    -o "${output_dir}/snake-fallback.sixel" \
    >"${log_file}" 2>&1 || true

if [ -s "${output_dir}/snake-fallback.sixel" ]; then
    printf 'ok 1 - fallback output produced data\n'
else
    printf 'not ok 1 - fallback output empty\n'
    exit 1
fi