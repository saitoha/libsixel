#!/bin/sh
# Shared lsqa helpers for palette quality TAP tests.
#
# The thresholds are intentionally lenient to catch obvious breakage such as
# black frames while remaining stable across palette strategies.

set -eu

if [ -n "${PALETTE_LSQA_HELPER_DIR-}" ]; then
    helper_root=${PALETTE_LSQA_HELPER_DIR}
else
    helper_root=$(CDPATH=; cd "$(dirname "$0")" && pwd)
fi
lsqa_common_path="${helper_root}/../regression/lsqa_common.sh"
. "${lsqa_common_path}"

PALETTE_LSQA_MS_SSIM_FLOOR=${PALETTE_LSQA_MS_SSIM_FLOOR:-0.6}
PALETTE_LSQA_PSNR_FLOOR=${PALETTE_LSQA_PSNR_FLOOR:-20.0}

palette_lsqa_init() {
    lsqa_init "$1"
}

palette_lsqa_run() {
    ref_path=$1
    out_path=$2
    stdout_path=$3
    stderr_path=$4
    status=0

    : >"${stdout_path}"
    : >"${stderr_path}"

    env LSQA_RANDOM_SEED="${LSQA_SEED}" ${SIXEL_RUNTIME-} "${LSQA_BIN}" \
        "${ref_path}" "${out_path}" >"${stdout_path}" \
        2>"${stderr_path}" || status=$?

    if [ ${status} -eq 126 ]; then
        : >"${stdout_path}"
        : >"${stderr_path}"
        env LSQA_RANDOM_SEED="${LSQA_SEED}" /bin/sh -c \
            'exec "$0" "$1" "$2"' ${SIXEL_RUNTIME-} "${LSQA_BIN}" \
            "${ref_path}" "${out_path}" >"${stdout_path}" \
            2>"${stderr_path}" || status=$?
    fi

    printf '%s' "${status}"
}

palette_lsqa_assert_quality() {
    ref_path=$1
    out_path=$2
    label=$3
    artifact_dir=$4

    out_file="${artifact_dir}/lsqa.json"
    err_file="${artifact_dir}/lsqa.err"
    run_status=$(palette_lsqa_run "${ref_path}" "${out_path}" \
        "${out_file}" "${err_file}")
    if [ ${run_status} -ne 0 ]; then
        printf '%s: assessment/lsqa returned %s: %s\n' \
            "${label}" "${run_status}" "$(cat "${err_file}")" >&2
        return 1
    fi

    ms_val=$(lsqa_parse_metric "MS-SSIM" "${out_file}")
    psnr_val=$(lsqa_parse_metric "PSNR_Y" "${out_file}")

    if lsqa_below_floor "${ms_val}" "${PALETTE_LSQA_MS_SSIM_FLOOR}"; then
        printf '%s: MS-SSIM %s below floor %s\n' \
            "${label}" "${ms_val}" "${PALETTE_LSQA_MS_SSIM_FLOOR}" >&2
        return 1
    fi
    if lsqa_below_floor "${psnr_val}" "${PALETTE_LSQA_PSNR_FLOOR}"; then
        printf '%s: PSNR_Y %s below floor %s\n' \
            "${label}" "${psnr_val}" "${PALETTE_LSQA_PSNR_FLOOR}" >&2
        return 1
    fi

    printf 'MS-SSIM=%s PSNR_Y=%s\n' "${ms_val}" "${psnr_val}" \
        >"${artifact_dir}/lsqa_metrics.txt"

    return 0
}
