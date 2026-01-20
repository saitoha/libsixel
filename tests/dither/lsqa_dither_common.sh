#!/bin/sh
# Shared lsqa helpers for dither quality TAP tests.
#
# These helpers focus on verifying that dithering output remains visually
# similar to a 64x64 reference image while keeping the per-test scripts small.

set -eu

if [ -n "${DITHER_LSQA_HELPER_DIR-}" ]; then
    helper_root=${DITHER_LSQA_HELPER_DIR}
else
    helper_root=$(CDPATH=; cd "$(dirname "$0")" && pwd)
fi
lsqa_common_path="${helper_root}/../regression/lsqa_common.sh"
. "${lsqa_common_path}"

DITHER_LSQA_MS_SSIM_FLOOR=${DITHER_LSQA_MS_SSIM_FLOOR:-0.6}
DITHER_LSQA_PSNR_FLOOR=${DITHER_LSQA_PSNR_FLOOR:-20.0}

dither_lsqa_init() {
    lsqa_init "$1"
}

dither_lsqa_run() {
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

dither_lsqa_assert_quality() {
    ref_path=$1
    out_path=$2
    label=$3
    artifact_dir=$4

    out_file="${artifact_dir}/lsqa.json"
    err_file="${artifact_dir}/lsqa.err"
    run_status=$(dither_lsqa_run "${ref_path}" "${out_path}" \
        "${out_file}" "${err_file}")
    if [ ${run_status} -ne 0 ]; then
        printf '%s: assessment/lsqa returned %s: %s\n' \
            "${label}" "${run_status}" "$(cat "${err_file}")" >&2
        return 1
    fi

    ms_val=$(lsqa_parse_metric "MS-SSIM" "${out_file}")
    psnr_val=$(lsqa_parse_metric "PSNR_Y" "${out_file}")

    if lsqa_below_floor "${ms_val}" "${DITHER_LSQA_MS_SSIM_FLOOR}"; then
        printf '%s: MS-SSIM %s below floor %s\n' \
            "${label}" "${ms_val}" "${DITHER_LSQA_MS_SSIM_FLOOR}" >&2
        return 1
    fi
    if lsqa_below_floor "${psnr_val}" "${DITHER_LSQA_PSNR_FLOOR}"; then
        printf '%s: PSNR_Y %s below floor %s\n' \
            "${label}" "${psnr_val}" "${DITHER_LSQA_PSNR_FLOOR}" >&2
        return 1
    fi

    printf 'MS-SSIM=%s PSNR_Y=%s\n' "${ms_val}" "${psnr_val}" \
        >"${artifact_dir}/lsqa_metrics.txt"

    return 0
}
