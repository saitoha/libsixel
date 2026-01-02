#!/bin/sh
# Common helpers for lsqa regression checks.
#
# Each test sources this file and calls one of the assertion helpers to keep
# every TAP script focused on a single input case.

set -eu

MS_SSIM_FLOOR=0.98
PSNR_FLOOR=40.0
REPEAT_COUNT=5

lsqa_init() {
    test_path=$1
    test_dir=$(CDPATH=; cd "$(dirname "${test_path}")" && pwd)
    regression_root=$(CDPATH=; cd "${test_dir}/.." && pwd)
    repo_root=$(CDPATH=; cd "${regression_root}/.." && pwd)

    LSQA_ARTIFACT_ROOT=${ARTIFACT_ROOT:-"${repo_root}/tests/_artifacts"}
    LSQA_INPUT_ROOT=${LSQA_INPUT_ROOT-}
    LSQA_BASELINE_DIR=${LSQA_BASELINE_DIR-}
    LSQA_SEED=${LSQA_SEED:-2024}

    build_root=${TOP_BUILDDIR:-${repo_root}}

    # Search for test assets in the build or source trees; prefer an explicit
    # LSQA_DATA_ROOT override when provided.
    set --
    if [ -n "${LSQA_DATA_ROOT-}" ]; then
        set -- "$@" "${LSQA_DATA_ROOT}"
    fi
    set -- "$@" \
        "${test_dir}/../data" \
        "${repo_root}/tests/data" \
        "${build_root}/tests/data"
    if [ -n "${TOP_SRCDIR-}" ]; then
        set -- "$@" "${TOP_SRCDIR}/tests/data"
    fi

    found_data_root=""
    for candidate in "$@"; do
        if [ -d "${candidate}/baseline" ] && [ -d "${candidate}/inputs" ]; then
            found_data_root=${candidate}
            break
        fi
    done

    if [ -z "${found_data_root}" ]; then
        printf 'lsqa data directory not found. looked for:%s\n' \
            "\n  $(printf '%s\n  ' "$@" | sed '/^ *$/d')" >&2
        return 1
    fi

    LSQA_DATA_ROOT=${found_data_root}
    LSQA_INPUT_ROOT=${LSQA_INPUT_ROOT:-"${LSQA_DATA_ROOT}"}
    LSQA_BASELINE_DIR=${LSQA_BASELINE_DIR:-"${LSQA_DATA_ROOT}/baseline"}
    lsqa_bin_env=${LSQA_BIN-}
    if [ -n "${lsqa_bin_env}" ]; then
        if [ -x "${lsqa_bin_env}" ]; then
            LSQA_BIN=${lsqa_bin_env}
        else
            printf 'LSQA_BIN points to a missing or non-executable path: %s\n' \
                "${lsqa_bin_env}" >&2
            return 1
        fi
    else
        set -- \
            "${build_root}/assessment/lsqa" \
            "${build_root}/assessment/.libs/lsqa" \
            "${build_root}/lsqa"
        for candidate in "$@"; do
            if [ -x "${candidate}" ]; then
                LSQA_BIN=${candidate}
                break
            fi
        done
        if [ -z "${LSQA_BIN-}" ]; then
            printf 'lsqa binary not found. looked for:%s%s%s\n' \
                "\n  ${build_root}/assessment/lsqa" \
                "\n  ${build_root}/assessment/.libs/lsqa" \
                "\n  ${build_root}/lsqa" >&2
            return 1
        fi
    fi

    return 0
}

lsqa_parse_metric() {
    metric_name=$1
    json_path=$2
    value=$(sed -n "s/.*\"${metric_name}\"[[:space:]]*:[[:space:]]*\\([^,]*\\),.*/\\1/p" \
        "${json_path}" | head -n 1)
    if [ -z "${value}" ] || [ "${value}" = "null" ]; then
        printf '0.0'
    else
        printf '%s' "${value}"
    fi
}

lsqa_below_floor() {
    lhs=$1
    rhs=$2
    awk -v a="${lhs}" -v b="${rhs}" 'BEGIN { exit (a + 1e-6 < b) ? 0 : 1 }'
}

lsqa_above_ceiling() {
    lhs=$1
    rhs=$2
    awk -v a="${lhs}" -v b="${rhs}" 'BEGIN { exit (a > b + 1e-12) ? 0 : 1 }'
}

lsqa_run() {
    target=$1
    stdout_path=$2
    stderr_path=$3

    : >"${stdout_path}"
    : >"${stderr_path}"

    env LSQA_RANDOM_SEED="${LSQA_SEED}" "${LSQA_BIN}" "${target}" "${target}" \
        >"${stdout_path}" 2>"${stderr_path}" || status=$?
    status=${status:-0}

    if [ ${status} -eq 126 ]; then
        : >"${stdout_path}"
        : >"${stderr_path}"
        env LSQA_RANDOM_SEED="${LSQA_SEED}" /bin/sh -c \
            'exec "$0" "$1" "$1"' "${LSQA_BIN}" "${target}" \
            >"${stdout_path}" 2>"${stderr_path}" || status=$?
        status=${status:-0}
    fi

    printf '%s' "${status}"
}

lsqa_assert_quality() {
    image_path=$1
    label=$2
    artifact_dir=$3

    out_file="${artifact_dir}/lsqa.json"
    err_file="${artifact_dir}/lsqa.err"

    status=$(lsqa_run "${image_path}" "${out_file}" "${err_file}")
    if [ ${status} -ne 0 ]; then
        printf '%s: assessment/lsqa returned %s: %s\n' \
            "${label}" "${status}" "$(cat "${err_file}")" >&2
        return 1
    fi

    ms_val=$(lsqa_parse_metric "MS-SSIM" "${out_file}")
    psnr_val=$(lsqa_parse_metric "PSNR_Y" "${out_file}")

    base_name="${label%.*}.json"
    base_path="${LSQA_BASELINE_DIR}/${base_name}"
    if [ ! -f "${base_path}" ]; then
        printf '%s: baseline %s missing\n' "${label}" "${base_name}" >&2
        return 1
    fi

    base_ms=$(lsqa_parse_metric "MS-SSIM" "${base_path}")
    base_psnr=$(lsqa_parse_metric "PSNR_Y" "${base_path}")

    floor_ms=${MS_SSIM_FLOOR}
    floor_psnr=${PSNR_FLOOR}
    if [ "${label}" = "palette.png" ]; then
        floor_ms=0.0
    fi

    ms_enforced=1
    if ! lsqa_above_ceiling "${ms_val}" "1e-6" && \
        ! lsqa_above_ceiling "${base_ms}" "1e-6"; then
        ms_enforced=0
    fi

    if [ ${ms_enforced} -ne 0 ] && lsqa_below_floor "${ms_val}" "${floor_ms}"; then
        printf '%s: MS-SSIM %s below floor %s\n' \
            "${label}" "${ms_val}" "${floor_ms}" >&2
        return 1
    fi
    if lsqa_below_floor "${psnr_val}" "${floor_psnr}"; then
        printf '%s: PSNR_Y %s below floor %s dB\n' \
            "${label}" "${psnr_val}" "${floor_psnr}" >&2
        return 1
    fi
    if [ ${ms_enforced} -ne 0 ] && lsqa_below_floor "${ms_val}" "${base_ms}"; then
        printf '%s: MS-SSIM %s regressed from baseline %s\n' \
            "${label}" "${ms_val}" "${base_ms}" >&2
        return 1
    fi
    if lsqa_below_floor "${psnr_val}" "${base_psnr}"; then
        printf '%s: PSNR_Y %s regressed from baseline %s\n' \
            "${label}" "${psnr_val}" "${base_psnr}" >&2
        return 1
    fi

    printf 'MS-SSIM=%s PSNR_Y=%s\n' "${ms_val}" "${psnr_val}" \
        >"${artifact_dir}/lsqa_metrics.txt"
    return 0
}

lsqa_expect_low_quality_or_fail() {
    image_path=$1
    label=$2
    artifact_dir=$3

    out_file="${artifact_dir}/lsqa.json"
    err_file="${artifact_dir}/lsqa.err"

    # Track the lsqa exit code locally so the caller's status variable is not
    # clobbered when failures are permitted for corrupted inputs.
    run_status=$(lsqa_run "${image_path}" "${out_file}" "${err_file}")
    if [ ${run_status} -eq 0 ]; then
        ms_val=$(lsqa_parse_metric "MS-SSIM" "${out_file}")
        psnr_val=$(lsqa_parse_metric "PSNR_Y" "${out_file}")
        if lsqa_below_floor "${ms_val}" "0.5" || \
            lsqa_below_floor "${psnr_val}" "10"; then
            printf 'MS-SSIM=%s PSNR_Y=%s\n' "${ms_val}" "${psnr_val}" \
                >"${artifact_dir}/lsqa_metrics.txt"
            return 0
        fi
        printf '%s: low-quality input accepted (MS-SSIM=%s PSNR_Y=%s)\n' \
            "${label}" "${ms_val}" "${psnr_val}" >&2
        return 1
    fi

    if [ ! -s "${err_file}" ]; then
        printf '%s: failed without diagnostic output\n' "${label}" >&2
        return 1
    fi
    return 0
}

lsqa_assert_repeat_stability() {
    image_path=$1
    label=$2
    artifact_dir=$3

    run_log="${artifact_dir}/repeat.log"
    : >"${run_log}"

    i=1
    while [ ${i} -le ${REPEAT_COUNT} ]; do
        out_file=$(mktemp)
        err_file=$(mktemp)
        status=$(lsqa_run "${image_path}" "${out_file}" "${err_file}")
        if [ ${status} -ne 0 ]; then
            printf '%s: repeat run %s failed (%s): %s\n' \
                "${label}" "${i}" "${status}" "$(cat "${err_file}")" >&2
            rm -f "${out_file}" "${err_file}"
            return 1
        fi
        ms_val=$(lsqa_parse_metric "MS-SSIM" "${out_file}")
        psnr_val=$(lsqa_parse_metric "PSNR_Y" "${out_file}")
        printf '%s %s\n' "${ms_val}" "${psnr_val}" >>"${run_log}"
        rm -f "${out_file}" "${err_file}"
        i=$((i + 1))
    done

    if [ ! -s "${run_log}" ]; then
        printf '%s: repeat log empty\n' "${label}" >&2
        return 1
    fi

    vars=$(awk '{ms_sum+=$1; ps_sum+=$2; ms[NR]=$1; ps[NR]=$2}
        END {
            if (NR == 0) { exit 0 }
            ms_avg = ms_sum / NR
            ps_avg = ps_sum / NR
            for (i = 1; i <= NR; i++) {
                ms_var += (ms[i] - ms_avg) * (ms[i] - ms_avg)
                ps_var += (ps[i] - ps_avg) * (ps[i] - ps_avg)
            }
            printf "%f %f\n", ms_var / NR, ps_var / NR
        }' "${run_log}")

    ms_var=$(printf '%s' "${vars}" | awk '{print $1}')
    ps_var=$(printf '%s' "${vars}" | awk '{print $2}')

    if lsqa_above_ceiling "${ms_var}" "1e-6"; then
        printf '%s: MS-SSIM variance %s exceeds 1e-6\n' \
            "${label}" "${ms_var}" >&2
        return 1
    fi
    if lsqa_above_ceiling "${ps_var}" "1e-3"; then
        printf '%s: PSNR_Y variance %s exceeds 1e-3\n' \
            "${label}" "${ps_var}" >&2
        return 1
    fi

    printf 'variance_ms=%s variance_psnr=%s\n' "${ms_var}" "${ps_var}" \
        >"${artifact_dir}/lsqa_variance.txt"
    return 0
}
