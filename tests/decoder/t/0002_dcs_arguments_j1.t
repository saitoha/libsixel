#!/bin/sh
# TAP test validating img2sixel DCS argument permutations with j=1.

# Enable strict mode with verbose tracing for diagnostics.
set -euxv

test_name=$(basename "$0")
test_dir=$(CDPATH=; cd "$(dirname "$0")" && pwd)
category_name=$(basename "$(dirname "${test_dir}")")
artifact_root=${ARTIFACT_ROOT:-"$(pwd)/_artifacts"}
artifact_dir="${artifact_root}/${category_name}/${test_name}"
log_file="${artifact_dir}/dcs-arguments.log"
output_dir="${artifact_dir}/outputs"

mkdir -p "${output_dir}"

script_dir=$(CDPATH=; cd "$(dirname "$0")" && pwd)
. "${script_dir}/../../common/t/0001_converters_common.t"

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

echo "1..11"

map8_png="${images_dir}/map8.png"

require_file "${map8_png}"

i=0
j=1
while [ "${i}" -le 10 ]; do
    stage_file="${output_dir}/stage-${i}-${j}.sixel"
    output_file="${output_dir}/dcs-${i}-${j}.sixel"

    if run_img2sixel "${map8_png}" >"${stage_file}" \
            2>>"${log_file}" && \
            sed "s/Pq/P${i};;${j}q/" "${stage_file}" | \
            run_img2sixel >"${output_file}" 2>>"${log_file}"; then
        pass ${case_id} "DCS prefix ${i};${j} accepted"
    else
        fail ${case_id} "DCS prefix ${i};${j} rejected"
    fi

    case_id=$((case_id + 1))
    i=$((i + 1))
done

exit "${status}"

