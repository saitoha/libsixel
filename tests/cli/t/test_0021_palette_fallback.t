#!/bin/sh
# TAP test ensuring palette expansion falls back to the shift path when
# lookup tables are intentionally disabled via the environment.

# Enable strict mode with verbose tracing for diagnostics.
set -euxv

test_name=$(basename "$0")
artifact_root=${ARTIFACT_ROOT:-"$(pwd)/_artifacts"}
artifact_dir="${artifact_root}/${test_name}"
log_file="${artifact_dir}/palette-fallback.log"
output_dir="${artifact_dir}/outputs"

tmp_dir="${artifact_dir}/tmp"

mkdir -p "${output_dir}" "${tmp_dir}"

script_dir=$(CDPATH=; cd "$(dirname "$0")" && pwd)
. "${script_dir}/converters-common.t"

status=0
case_id=1

ensure_converter_available "IMG2SIXEL" "${IMG2SIXEL_PATH}" "img2sixel"

pass() {
    printf 'ok %s - %s\n' "$1" "$2"
}

fail() {
    printf 'not ok %s - %s\n' "$1" "$2"
    status=1
}

echo "1..2"

snake_ascii_pbm="${images_dir}/snake-ascii.pbm"
require_file "${snake_ascii_pbm}"

if SIXEL_PALETTE_DISABLE_TABLES=1 \
        run_img2sixel "${snake_ascii_pbm}" \
        -o "${output_dir}/snake-fallback.sixel" \
        >"${log_file}" 2>&1; then
    pass ${case_id} "PAL1 input expands via fallback path"
else
    fail ${case_id} "fallback expansion failed"
fi
case_id=$((case_id + 1))

if [ -s "${output_dir}/snake-fallback.sixel" ]; then
    pass ${case_id} "fallback output produced data"
else
    fail ${case_id} "fallback output empty"
fi

exit "${status}"
