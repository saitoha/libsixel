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
    elif ! runtime_exec "${LSQA_BIN}" "${lsqa_sixel_ref_path}" "${lsqa_sixel_sixel_path}" \
        >"${lsqa_sixel_out_path}" 2>"${lsqa_sixel_err_path}"; then
        lsqa_sixel_status=$?
    fi

    if [ ${lsqa_sixel_status} -eq 126 ]; then
        : >"${lsqa_sixel_out_path}"
        : >"${lsqa_sixel_err_path}"
        if ! run_img2sixel -Lbuiltin "${lsqa_sixel_ref_path}" \
            | /bin/sh -c 'exec "$0" "$1"' ${SIXEL_RUNTIME-} \
            "${LSQA_BIN}" "${lsqa_sixel_ref_path}" \
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
    lsqa_sixel_out_file="${lsqa_sixel_artifact_dir}/lsqa.json"
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

    # Persist every lsqa.json run so flaky regressions can be diffed later.
    lsqa_sixel_stamp=$(date -u +%Y%m%dT%H%M%SZ)
    lsqa_sixel_history_file="${lsqa_sixel_history_dir}/lsqa.${lsqa_sixel_stamp}.$$".json
    mkdir -p "${lsqa_sixel_history_dir}"
    cp "${lsqa_sixel_out_file}" "${lsqa_sixel_history_file}"

    lsqa_sixel_ms_val=$(lsqa_parse_metric "MS-SSIM" \
        "${lsqa_sixel_out_file}")
    lsqa_sixel_psnr_val=$(lsqa_parse_metric "PSNR_Y" \
        "${lsqa_sixel_out_file}")

    lsqa_sixel_base_name="${lsqa_sixel_label%.*}.json"
    lsqa_sixel_base_path="${LSQA_BASELINE_DIR}/${lsqa_sixel_base_name}"
    if [ ! -f "${lsqa_sixel_base_path}" ]; then
        printf '%s: baseline %s missing\n' "${lsqa_sixel_label}" \
            "${lsqa_sixel_base_name}" >&2
        return 1
    fi

    lsqa_sixel_base_ms=$(lsqa_parse_metric "MS-SSIM" \
        "${lsqa_sixel_base_path}")
    lsqa_sixel_base_psnr=$(lsqa_parse_metric "PSNR_Y" \
        "${lsqa_sixel_base_path}")

    lsqa_sixel_floor_ms=${MS_SSIM_FLOOR}
    lsqa_sixel_floor_psnr=${PSNR_FLOOR}

    lsqa_sixel_ms_enforced=1
    if ! lsqa_above_ceiling "${lsqa_sixel_ms_val}" "1e-6" && \
        ! lsqa_above_ceiling "${lsqa_sixel_base_ms}" "1e-6"; then
        lsqa_sixel_ms_enforced=0
    fi

    if [ ${lsqa_sixel_ms_enforced} -ne 0 ] && \
        lsqa_below_floor "${lsqa_sixel_ms_val}" \
            "${lsqa_sixel_floor_ms}"; then
        printf '%s: MS-SSIM %s below floor %s\n' \
            "${lsqa_sixel_label}" "${lsqa_sixel_ms_val}" \
            "${lsqa_sixel_floor_ms}" >&2
        return 1
    fi
    if lsqa_below_floor "${lsqa_sixel_psnr_val}" \
        "${lsqa_sixel_floor_psnr}"; then
        printf '%s: PSNR_Y %s below floor %s dB\n' \
            "${lsqa_sixel_label}" "${lsqa_sixel_psnr_val}" \
            "${lsqa_sixel_floor_psnr}" >&2
        return 1
    fi
    if [ ${lsqa_sixel_ms_enforced} -ne 0 ] && \
        lsqa_below_floor "${lsqa_sixel_ms_val}" \
            "${lsqa_sixel_base_ms}"; then
        printf '%s: MS-SSIM %s regressed from baseline %s\n' \
            "${lsqa_sixel_label}" "${lsqa_sixel_ms_val}" \
            "${lsqa_sixel_base_ms}" >&2
        return 1
    fi
    if lsqa_below_floor "${lsqa_sixel_psnr_val}" \
        "${lsqa_sixel_base_psnr}"; then
        printf '%s: PSNR_Y %s regressed from baseline %s\n' \
            "${lsqa_sixel_label}" "${lsqa_sixel_psnr_val}" \
            "${lsqa_sixel_base_psnr}" >&2
        return 1
    fi

    printf 'MS-SSIM=%s PSNR_Y=%s\n' "${lsqa_sixel_ms_val}" \
        "${lsqa_sixel_psnr_val}" \
        >"${lsqa_sixel_artifact_dir}/lsqa_metrics.txt"
    return 0
}
