#!/bin/sh
# TAP test: animation disabled with update mode flags.

set -euxv

test_name=$(basename "$0")
artifact_root=${ARTIFACT_ROOT:-"$(pwd)/_artifacts"}
artifact_dir="${artifact_root}/${test_name}"
log_file="${artifact_dir}/animation.log"
output_file="${artifact_dir}/disable-update.sixel"

mkdir -p "${artifact_dir}"

script_dir=$(CDPATH=; cd "$(dirname "$0")" && pwd)
. "${script_dir}/../../common/t/0001_converters_common.t"

status=0

ensure_converter_available "IMG2SIXEL" "${IMG2SIXEL_PATH}" "img2sixel"

pass() {
    printf 'ok %s - %s\n' "$1" "$2"
}

fail() {
    printf 'not ok %s - %s\n' "$1" "$2"
    status=1
}

echo "1..1"

seq_gif="${images_dir}/seq2gif.gif"

require_file "${seq_gif}"

if run_img2sixel -ldisable -dnone -u -lauto "${seq_gif}" >"${output_file}" 2>>"${log_file}"; then
    pass 1 "animation disabled with update mode"
else
    fail 1 "animation disable with update failed"
fi

exit "${status}"
