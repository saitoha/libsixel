#!/bin/sh
# Common helpers for lsqa regression checks.
#
# Each test sources this file and calls one of the assertion helpers to keep
# every TAP script focused on a single input case.
#
# Helper layout:
# - Assertions compare MS-SSIM values to caller-provided floors.
# - Repeat checks evaluate variance from lsqa output.

set -eu

MS_SSIM_FLOOR=${LSQA_MS_SSIM_FLOOR:-0.98}
REPEAT_COUNT=5

. "${TOP_SRCDIR}/tests/lib/sh/common/tap.sh"
. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"


_lsqa_read_ms_ssim() {
    output_path=$1
    # Read a single-line MS-SSIM value while guarding against NaN.
    value=$(awk 'NR == 1 {
        gsub(/^[[:space:]]+|[[:space:]]+$/, "", $0)
        lower = tolower($0)
        if (lower == "" || lower == "nan") {
            print "0.0"
        } else {
            print $1
        }
    }' "${output_path}")
    printf '%s' "${value}"
}

_lsqa_above_ceiling() {
    lhs=$1
    rhs=$2
    awk -v a="${lhs}" -v b="${rhs}" 'BEGIN { exit (a > b + 1e-12) ? 0 : 1 }'
}

_lsqa_run_compare() {
    lsqa_run_ref_path=$1
    lsqa_run_out_path=$2
    lsqa_run_stdout_path=$3
    lsqa_run_stderr_path=$4
    lsqa_run_status=0

    : >"${lsqa_run_stdout_path}"
    : >"${lsqa_run_stderr_path}"

    run_lsqa -m MS-SSIM "${lsqa_run_ref_path}" "${lsqa_run_out_path}" \
        >"${lsqa_run_stdout_path}" \
        2>"${lsqa_run_stderr_path}" || lsqa_run_status=$?

    printf '%s' "${lsqa_run_status}"
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
        lsqa_repeat_run_status=$(_lsqa_run_compare \
            "${lsqa_repeat_image_path}" \
            "${lsqa_repeat_image_path}" "${lsqa_repeat_out_file}" \
            "${lsqa_repeat_err_file}")
        if [ ${lsqa_repeat_run_status} -ne 0 ]; then
            printf '%s: repeat run %s failed (%s): %s\n' \
                "${lsqa_repeat_label}" "${lsqa_repeat_i}" \
                "${lsqa_repeat_run_status}" \
                "$(cat "${lsqa_repeat_err_file}")" >&2
            rm -f "${lsqa_repeat_out_file}" "${lsqa_repeat_err_file}"
            return 1
        fi
        lsqa_repeat_ms_val=$(_lsqa_read_ms_ssim "${lsqa_repeat_out_file}")
        printf '%s\n' "${lsqa_repeat_ms_val}" \
            >>"${lsqa_repeat_run_log}"
        rm -f "${lsqa_repeat_out_file}" "${lsqa_repeat_err_file}"
        lsqa_repeat_i=$((lsqa_repeat_i + 1))
    done

    if [ ! -s "${lsqa_repeat_run_log}" ]; then
        printf '%s: repeat log empty\n' "${lsqa_repeat_label}" >&2
        return 1
    fi

    lsqa_repeat_vars=$(awk '{ms_sum+=$1; ms[NR]=$1}
        END {
            if (NR == 0) { exit 0 }
            ms_avg = ms_sum / NR
            for (i = 1; i <= NR; i++) {
                ms_var += (ms[i] - ms_avg) * (ms[i] - ms_avg)
            }
            printf "%f\n", ms_var / NR
        }' "${lsqa_repeat_run_log}")

    lsqa_repeat_ms_var=$(printf '%s' "${lsqa_repeat_vars}" | awk '{print $1}')

    if _lsqa_above_ceiling "${lsqa_repeat_ms_var}" "1e-6"; then
        printf '%s: MS-SSIM variance %s exceeds 1e-6\n' \
            "${lsqa_repeat_label}" "${lsqa_repeat_ms_var}" >&2
        return 1
    fi
    printf 'variance_ms=%s\n' \
        "${lsqa_repeat_ms_var}" \
        >"${lsqa_repeat_artifact_dir}/lsqa_variance.txt"
    return 0
}
