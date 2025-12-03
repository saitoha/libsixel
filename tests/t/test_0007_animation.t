#!/bin/sh
# TAP test exercising img2sixel animation-related flags.

set -eu

test_name=$(basename "$0")
artifact_root=${ARTIFACT_ROOT:-"$(pwd)/_artifacts"}
artifact_dir="${artifact_root}/${test_name}"
log_file="${artifact_dir}/animation.log"
output_dir="${artifact_dir}/outputs"

mkdir -p "${output_dir}"

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

echo "1..4"

seq_gif="${images_dir}/seq2gif.gif"

require_file "${seq_gif}"

if run_img2sixel -ldisable -dnone -u -lauto "${seq_gif}" \
        >"${output_dir}/case01.sixel" 2>>"${log_file}"; then
    pass ${case_id} "animation disabled with update mode"
else
    fail ${case_id} "animation disable with update failed"
fi
case_id=$((case_id + 1))

if run_img2sixel -ldisable -dnone -g "${seq_gif}" \
        >"${output_dir}/case02.sixel" 2>>"${log_file}"; then
    pass ${case_id} "static frame rendering succeeds"
else
    fail ${case_id} "static frame rendering fails"
fi
case_id=$((case_id + 1))

if run_img2sixel -ldisable -dnone -u -g "${seq_gif}" \
        >"${output_dir}/case03.sixel" 2>>"${log_file}"; then
    pass ${case_id} "combined update and static frame works"
else
    fail ${case_id} "combined update and static frame fails"
fi
case_id=$((case_id + 1))

if run_img2sixel -S -datkinson "${seq_gif}" >"${output_dir}/case04.sixel" \
        2>>"${log_file}"; then
    pass ${case_id} "sequence splitting with Atkinson works"
else
    fail ${case_id} "sequence splitting with Atkinson fails"
fi

exit "${status}"
