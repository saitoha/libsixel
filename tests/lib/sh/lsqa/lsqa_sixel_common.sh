#!/bin/sh
# Common helpers for lsqa checks that compare sixel output to a reference.

set -eu

lsqa_sixel_common_path=${lsqa_sixel_common_path:-"$0"}
lsqa_sixel_root=${LSQA_SIXEL_HELPER_DIR-}
if [ -z "${lsqa_sixel_root}" ]; then
    lsqa_sixel_root=$(CDPATH=; cd "$(dirname "${lsqa_sixel_common_path}")" && pwd)
fi

. "${lsqa_sixel_root}/lsqa_common.sh"
. "${lsqa_sixel_root}/../../../common/t/0001_converters_common.t"

lsqa_sixel_init() {
    test_path=$1

    if ! lsqa_init "${test_path}"; then
        return 1
    fi

    ensure_converter_available "IMG2SIXEL" "${IMG2SIXEL_PATH}" "img2sixel"

    return 0
}

lsqa_sixel_run() {
    lsqa_sixel_ref_path=$1
    lsqa_sixel_sixel_path=$2
    lsqa_sixel_out_path=$3
    lsqa_sixel_err_path=$4
    lsqa_sixel_status=0

    : >"${lsqa_sixel_out_path}"
    : >"${lsqa_sixel_err_path}"

    if ! run_img2sixel -Lbuiltin "${lsqa_sixel_ref_path}" > "${lsqa_sixel_sixel_path}"; then
        lsqa_sixel_status=$?
    elif ! runtime_exec "${LSQA_BIN}" -m MS-SSIM \
        "${lsqa_sixel_ref_path}" "${lsqa_sixel_sixel_path}" \
        >"${lsqa_sixel_out_path}" 2>"${lsqa_sixel_err_path}"; then
        lsqa_sixel_status=$?
    fi

    if [ ${lsqa_sixel_status} -eq 126 ]; then
        : >"${lsqa_sixel_out_path}"
        : >"${lsqa_sixel_err_path}"
        if ! run_img2sixel -Lbuiltin "${lsqa_sixel_ref_path}" \
            | /bin/sh -c 'exec "$0" -m MS-SSIM "$1"' \
            ${SIXEL_RUNTIME-} "${LSQA_BIN}" "${lsqa_sixel_ref_path}" \
            >"${lsqa_sixel_out_path}" 2>"${lsqa_sixel_err_path}"; then
            lsqa_sixel_status=$?
        fi
    fi

    printf '%s' "${lsqa_sixel_status}"
}

lsqa_sixel_assert_quality() {
    lsqa_sixel_ref_path=$1
    lsqa_sixel_label=$2
    lsqa_sixel_artifact_dir=$3

    lsqa_sixel_sixel_file="${lsqa_sixel_artifact_dir}/output.six"
    lsqa_sixel_out_file="${lsqa_sixel_artifact_dir}/lsqa.txt"
    lsqa_sixel_err_file="${lsqa_sixel_artifact_dir}/lsqa.err"
    lsqa_sixel_history_dir="${lsqa_sixel_artifact_dir}/lsqa-history"
    lsqa_sixel_stamp=""
    lsqa_sixel_history_file=""

    lsqa_sixel_run_status=$(lsqa_sixel_run "${lsqa_sixel_ref_path}" \
        "${lsqa_sixel_sixel_file}" "${lsqa_sixel_out_file}" "${lsqa_sixel_err_file}")
    if [ ${lsqa_sixel_run_status} -ne 0 ]; then
        printf '%s: assessment/lsqa returned %s: %s\n' \
            "${lsqa_sixel_label}" "${lsqa_sixel_run_status}" \
            "$(cat "${lsqa_sixel_err_file}")" >&2
        return 1
    fi

    # Persist every lsqa.txt run so flaky regressions can be diffed later.
    lsqa_sixel_stamp=$(date -u +%Y%m%dT%H%M%SZ)
    lsqa_sixel_history_file="${lsqa_sixel_history_dir}/lsqa.${lsqa_sixel_stamp}.$$".txt
    mkdir -p "${lsqa_sixel_history_dir}"
    cp "${lsqa_sixel_out_file}" "${lsqa_sixel_history_file}"

    lsqa_sixel_ms_val=$(lsqa_read_ms_ssim "${lsqa_sixel_out_file}")

    lsqa_sixel_floor_ms=${MS_SSIM_FLOOR}
    if lsqa_below_floor "${lsqa_sixel_ms_val}" \
        "${lsqa_sixel_floor_ms}"; then
        printf '%s: MS-SSIM %s below floor %s\n' \
            "${lsqa_sixel_label}" "${lsqa_sixel_ms_val}" \
            "${lsqa_sixel_floor_ms}" >&2
        return 1
    fi

    printf 'MS-SSIM=%s\n' "${lsqa_sixel_ms_val}" \
        >"${lsqa_sixel_artifact_dir}/lsqa_metrics.txt"
    return 0
}
