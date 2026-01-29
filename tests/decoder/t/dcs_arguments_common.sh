#!/bin/sh
# Shared helpers for DCS argument TAP tests.

# Run a single DCS argument test case with the provided i and j values.
dcs_arguments_run() {
    i=$1
    j=$2

    test_name=$(basename "$0")
    test_dir=$(CDPATH=; cd "$(dirname "$0")" && pwd)
    category_name=$(basename "$(dirname "${test_dir}")")
    artifact_root=${ARTIFACT_ROOT:-"$(pwd)/_artifacts"}
    artifact_dir="${artifact_root}/${category_name}/${test_name}"
    log_file="${artifact_dir}/dcs-arguments.log"
    output_dir="${artifact_dir}/outputs"

    mkdir -p "${output_dir}"

    script_dir=$(CDPATH=; cd "$(dirname "$0")" && pwd)
    . "${script_dir}/../../_lib/sh/common.sh"

    status=0

    ensure_converter_available "IMG2SIXEL" "${IMG2SIXEL_PATH}" "img2sixel"

    echo "1..1"

    map8_png="${images_dir}/map8.png"
    require_file "${map8_png}"

    stage_file="${output_dir}/stage-${i}-${j}.sixel"
    output_file="${output_dir}/dcs-${i}-${j}.sixel"

    if run_img2sixel "${map8_png}" >"${stage_file}" \
            2>>"${log_file}" && \
            sed "s/Pq/P${i};;${j}q/" "${stage_file}" | \
            run_img2sixel >"${output_file}" 2>>"${log_file}"; then
        printf 'ok 1 - DCS prefix %s;%s accepted\n' "${i}" "${j}"
    else
        printf 'not ok 1 - DCS prefix %s;%s rejected\n' "${i}" "${j}"
        status=1
    fi

    exit "${status}"
}
