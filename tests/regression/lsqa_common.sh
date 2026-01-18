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
    # Use POSIX awk and index() to avoid regex quirks on Solaris awk.
    value=$(awk -v key="\"${metric_name}\"" '
        index($0, key) {
            line=$0
            pos=index(line, key)
            line=substr(line, pos + length(key))
            sub(/^[[:space:]]*:[[:space:]]*/, "", line)
            sub(/[ ,}].*$/, "", line)
            print line
            exit
        }
    ' "${json_path}")
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
    lsqa_run_target=$1
    lsqa_run_stdout_path=$2
    lsqa_run_stderr_path=$3
    lsqa_run_status=0

    : >"${lsqa_run_stdout_path}"
    : >"${lsqa_run_stderr_path}"

    env LSQA_RANDOM_SEED="${LSQA_SEED}" "${LSQA_BIN}" "${lsqa_run_target}" \
        "${lsqa_run_target}" >"${lsqa_run_stdout_path}" \
        2>"${lsqa_run_stderr_path}" || lsqa_run_status=$?

    if [ ${lsqa_run_status} -eq 126 ]; then
        : >"${lsqa_run_stdout_path}"
        : >"${lsqa_run_stderr_path}"
        env LSQA_RANDOM_SEED="${LSQA_SEED}" /bin/sh -c \
            'exec "$0" "$1" "$1"' "${LSQA_BIN}" "${lsqa_run_target}" \
            >"${lsqa_run_stdout_path}" 2>"${lsqa_run_stderr_path}" \
            || lsqa_run_status=$?
    fi

    printf '%s' "${lsqa_run_status}"
}

lsqa_assert_quality() {
    lsqa_quality_image_path=$1
    lsqa_quality_label=$2
    lsqa_quality_artifact_dir=$3

    lsqa_quality_out_file="${lsqa_quality_artifact_dir}/lsqa.json"
    lsqa_quality_err_file="${lsqa_quality_artifact_dir}/lsqa.err"

    lsqa_quality_run_status=$(lsqa_run "${lsqa_quality_image_path}" \
        "${lsqa_quality_out_file}" "${lsqa_quality_err_file}")
    if [ ${lsqa_quality_run_status} -ne 0 ]; then
        printf '%s: assessment/lsqa returned %s: %s\n' \
            "${lsqa_quality_label}" "${lsqa_quality_run_status}" \
            "$(cat "${lsqa_quality_err_file}")" >&2
        return 1
    fi

    lsqa_quality_ms_val=$(lsqa_parse_metric "MS-SSIM" \
        "${lsqa_quality_out_file}")
    lsqa_quality_psnr_val=$(lsqa_parse_metric "PSNR_Y" \
        "${lsqa_quality_out_file}")

    lsqa_quality_base_name="${lsqa_quality_label%.*}.json"
    lsqa_quality_base_path="${LSQA_BASELINE_DIR}/${lsqa_quality_base_name}"
    if [ ! -f "${lsqa_quality_base_path}" ]; then
        printf '%s: baseline %s missing\n' "${lsqa_quality_label}" \
            "${lsqa_quality_base_name}" >&2
        return 1
    fi

    lsqa_quality_base_ms=$(lsqa_parse_metric "MS-SSIM" \
        "${lsqa_quality_base_path}")
    lsqa_quality_base_psnr=$(lsqa_parse_metric "PSNR_Y" \
        "${lsqa_quality_base_path}")

    lsqa_quality_floor_ms=${MS_SSIM_FLOOR}
    lsqa_quality_floor_psnr=${PSNR_FLOOR}
    if [ "${lsqa_quality_label}" = "palette.png" ]; then
        lsqa_quality_floor_ms=0.0
    fi

    lsqa_quality_ms_enforced=1
    if ! lsqa_above_ceiling "${lsqa_quality_ms_val}" "1e-6" && \
        ! lsqa_above_ceiling "${lsqa_quality_base_ms}" "1e-6"; then
        lsqa_quality_ms_enforced=0
    fi

    if [ ${lsqa_quality_ms_enforced} -ne 0 ] && \
        lsqa_below_floor "${lsqa_quality_ms_val}" \
            "${lsqa_quality_floor_ms}"; then
        printf '%s: MS-SSIM %s below floor %s\n' \
            "${lsqa_quality_label}" "${lsqa_quality_ms_val}" \
            "${lsqa_quality_floor_ms}" >&2
        return 1
    fi
    if lsqa_below_floor "${lsqa_quality_psnr_val}" \
        "${lsqa_quality_floor_psnr}"; then
        printf '%s: PSNR_Y %s below floor %s dB\n' \
            "${lsqa_quality_label}" "${lsqa_quality_psnr_val}" \
            "${lsqa_quality_floor_psnr}" >&2
        return 1
    fi
    if [ ${lsqa_quality_ms_enforced} -ne 0 ] && \
        lsqa_below_floor "${lsqa_quality_ms_val}" \
            "${lsqa_quality_base_ms}"; then
        printf '%s: MS-SSIM %s regressed from baseline %s\n' \
            "${lsqa_quality_label}" "${lsqa_quality_ms_val}" \
            "${lsqa_quality_base_ms}" >&2
        return 1
    fi
    if lsqa_below_floor "${lsqa_quality_psnr_val}" \
        "${lsqa_quality_base_psnr}"; then
        printf '%s: PSNR_Y %s regressed from baseline %s\n' \
            "${lsqa_quality_label}" "${lsqa_quality_psnr_val}" \
            "${lsqa_quality_base_psnr}" >&2
        return 1
    fi

    printf 'MS-SSIM=%s PSNR_Y=%s\n' "${lsqa_quality_ms_val}" \
        "${lsqa_quality_psnr_val}" \
        >"${lsqa_quality_artifact_dir}/lsqa_metrics.txt"
    return 0
}

lsqa_expect_low_quality_or_fail() {
    lsqa_low_image_path=$1
    lsqa_low_label=$2
    lsqa_low_artifact_dir=$3

    lsqa_low_out_file="${lsqa_low_artifact_dir}/lsqa.json"
    lsqa_low_err_file="${lsqa_low_artifact_dir}/lsqa.err"

    lsqa_low_run_status=$(lsqa_run "${lsqa_low_image_path}" \
        "${lsqa_low_out_file}" "${lsqa_low_err_file}")
    if [ ${lsqa_low_run_status} -eq 0 ]; then
        lsqa_low_ms_val=$(lsqa_parse_metric "MS-SSIM" \
            "${lsqa_low_out_file}")
        lsqa_low_psnr_val=$(lsqa_parse_metric "PSNR_Y" \
            "${lsqa_low_out_file}")
        if lsqa_below_floor "${lsqa_low_ms_val}" "0.5" || \
            lsqa_below_floor "${lsqa_low_psnr_val}" "10"; then
            printf 'MS-SSIM=%s PSNR_Y=%s\n' "${lsqa_low_ms_val}" \
                "${lsqa_low_psnr_val}" \
                >"${lsqa_low_artifact_dir}/lsqa_metrics.txt"
            return 0
        fi
        printf '%s: low-quality input accepted (MS-SSIM=%s PSNR_Y=%s)\n' \
            "${lsqa_low_label}" "${lsqa_low_ms_val}" \
            "${lsqa_low_psnr_val}" >&2
        return 1
    fi

    if [ ! -s "${lsqa_low_err_file}" ]; then
        printf '%s: failed without diagnostic output\n' \
            "${lsqa_low_label}" >&2
        return 1
    fi
    return 0
}

lsqa_assert_repeat_stability() {
    lsqa_repeat_image_path=$1
    lsqa_repeat_label=$2
    lsqa_repeat_artifact_dir=$3

    lsqa_repeat_run_log="${lsqa_repeat_artifact_dir}/repeat.log"
    : >"${lsqa_repeat_run_log}"

    lsqa_repeat_i=1
    while [ ${lsqa_repeat_i} -le ${REPEAT_COUNT} ]; do
        lsqa_repeat_out_file=$(mktemp)
        lsqa_repeat_err_file=$(mktemp)
        lsqa_repeat_run_status=$(lsqa_run "${lsqa_repeat_image_path}" \
            "${lsqa_repeat_out_file}" "${lsqa_repeat_err_file}")
        if [ ${lsqa_repeat_run_status} -ne 0 ]; then
            printf '%s: repeat run %s failed (%s): %s\n' \
                "${lsqa_repeat_label}" "${lsqa_repeat_i}" \
                "${lsqa_repeat_run_status}" \
                "$(cat "${lsqa_repeat_err_file}")" >&2
            rm -f "${lsqa_repeat_out_file}" "${lsqa_repeat_err_file}"
            return 1
        fi
        lsqa_repeat_ms_val=$(lsqa_parse_metric "MS-SSIM" \
            "${lsqa_repeat_out_file}")
        lsqa_repeat_psnr_val=$(lsqa_parse_metric "PSNR_Y" \
            "${lsqa_repeat_out_file}")
        printf '%s %s\n' "${lsqa_repeat_ms_val}" "${lsqa_repeat_psnr_val}" \
            >>"${lsqa_repeat_run_log}"
        rm -f "${lsqa_repeat_out_file}" "${lsqa_repeat_err_file}"
        lsqa_repeat_i=$((lsqa_repeat_i + 1))
    done

    if [ ! -s "${lsqa_repeat_run_log}" ]; then
        printf '%s: repeat log empty\n' "${lsqa_repeat_label}" >&2
        return 1
    fi

    lsqa_repeat_vars=$(awk '{ms_sum+=$1; ps_sum+=$2; ms[NR]=$1; ps[NR]=$2}
        END {
            if (NR == 0) { exit 0 }
            ms_avg = ms_sum / NR
            ps_avg = ps_sum / NR
            for (i = 1; i <= NR; i++) {
                ms_var += (ms[i] - ms_avg) * (ms[i] - ms_avg)
                ps_var += (ps[i] - ps_avg) * (ps[i] - ps_avg)
            }
            printf "%f %f\n", ms_var / NR, ps_var / NR
        }' "${lsqa_repeat_run_log}")

    lsqa_repeat_ms_var=$(printf '%s' "${lsqa_repeat_vars}" | awk '{print $1}')
    lsqa_repeat_ps_var=$(printf '%s' "${lsqa_repeat_vars}" | awk '{print $2}')

    if lsqa_above_ceiling "${lsqa_repeat_ms_var}" "1e-6"; then
        printf '%s: MS-SSIM variance %s exceeds 1e-6\n' \
            "${lsqa_repeat_label}" "${lsqa_repeat_ms_var}" >&2
        return 1
    fi
    if lsqa_above_ceiling "${lsqa_repeat_ps_var}" "1e-3"; then
        printf '%s: PSNR_Y variance %s exceeds 1e-3\n' \
            "${lsqa_repeat_label}" "${lsqa_repeat_ps_var}" >&2
        return 1
    fi

    printf 'variance_ms=%s variance_psnr=%s\n' \
        "${lsqa_repeat_ms_var}" "${lsqa_repeat_ps_var}" \
        >"${lsqa_repeat_artifact_dir}/lsqa_variance.txt"
    return 0
}
