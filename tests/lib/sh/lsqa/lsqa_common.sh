#!/bin/sh
# Common helpers for lsqa regression checks.
#
# Each test sources this file and calls one of the assertion helpers to keep
# every TAP script focused on a single input case.
#
# Helper layout:
# - Initialization discovers lsqa binaries and data directories.
# - Assertions compare MS-SSIM to a caller-provided floor.
# - Sixel checks are handled by callers before asserting quality.
# - Baseline checks rely on lsqa exit codes.

set -eu

MS_SSIM_FLOOR=${LSQA_MS_SSIM_FLOOR:-0.98}
REPEAT_COUNT=5

lsqa_common_path=${lsqa_common_path:-"$0"}
lsqa_helper_root=${LSQA_HELPER_DIR-}
if [ -z "${lsqa_helper_root}" ]; then
    lsqa_helper_root=$(CDPATH=; cd "$(dirname "${lsqa_common_path}")" && pwd)
fi
. "${lsqa_helper_root}/../common/tap.sh"

_lsqa_require_converter_common() {
    if [ -n "${LSQA_CONVERTER_COMMON_LOADED-}" ]; then
        return 0
    fi

    # Load converter helpers lazily to avoid altering unrelated tests.
    . "${lsqa_helper_root}/../../../_lib/sh/common.sh"
    LSQA_CONVERTER_COMMON_LOADED=1
    return 0
}

lsqa_init() {
    test_path=$1
    test_dir=$(CDPATH=; cd "$(dirname "${test_path}")" && pwd)
    regression_root=$(CDPATH=; cd "${test_dir}/.." && pwd)
    repo_root=$(CDPATH=; cd "${regression_root}/.." && pwd)

    LSQA_ARTIFACT_ROOT=${ARTIFACT_ROOT:-"${repo_root}/tests/_artifacts"}
    LSQA_INPUT_ROOT=${LSQA_INPUT_ROOT-}
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
        if [ -d "${candidate}/inputs" ]; then
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
    lsqa_bin_env=${LSQA_BIN-}
    if [ -n "${lsqa_bin_env}" ]; then
        if [ -x "${lsqa_bin_env}" ]; then
            LSQA_BIN=${lsqa_bin_env}
            LSQA_PATH=${lsqa_bin_env}
        else
            printf 'LSQA_BIN points to a missing or non-executable path: %s\n' \
                "${lsqa_bin_env}" >&2
            return 1
        fi
    else
        set -- \
            "${build_root}/assessment/lsqa${SIXEL_BIN_EXT-}" \
            "${build_root}/assessment/.libs/lsqa${SIXEL_BIN_EXT-}" \
            "${build_root}/lsqa${SIXEL_BIN_EXT-}"
        for candidate in "$@"; do
            if [ -x "${candidate}" ]; then
                LSQA_BIN=${candidate}
                LSQA_PATH=${candidate}
                break
            fi
        done
        if [ -z "${LSQA_BIN-}" ]; then
            printf 'lsqa binary not found. looked for:%s%s%s\n' \
                "\n  ${build_root}/assessment/lsqa${SIXEL_BIN_EXT-}" \
                "\n  ${build_root}/assessment/.libs/lsqa${SIXEL_BIN_EXT-}" \
                "\n  ${build_root}/lsqa${SIXEL_BIN_EXT-}" >&2
            return 1
        fi
    fi

    _lsqa_require_converter_common

    return 0
}

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

# Compare two images and enforce an MS-SSIM floor.
#
# Arguments:
#  1) Reference image path (input)
#  2) Output image path (candidate)
#  3) Label used in diagnostics
#  4) Artifact directory (kept for call-site compatibility)
#  5) Optional MS-SSIM floor override (default: MS_SSIM_FLOOR)
lsqa_run_benchmark() {
    lsqa_quality_ref_path=$1
    lsqa_quality_out_path=$2
    lsqa_quality_label=$3
    lsqa_quality_artifact_dir=$4
    lsqa_quality_floor_ms=${5:-${MS_SSIM_FLOOR}}

    lsqa_quality_err_file=$(mktemp)
    lsqa_quality_run_status=0

    if ! run_lsqa -b "MS-SSIM:${lsqa_quality_floor_ms}" \
        "${lsqa_quality_ref_path}" "${lsqa_quality_out_path}" \
        > /dev/null 2>"${lsqa_quality_err_file}"; then
        lsqa_quality_run_status=$?
    fi

    if [ ${lsqa_quality_run_status} -ne 0 ]; then
        printf '# %s: assessment/lsqa returned %s\n' \
            "${lsqa_quality_label}" "${lsqa_quality_run_status}"
        if [ -s "${lsqa_quality_err_file}" ]; then
            printf '# lsqa stderr follows\n'
            sed 's/^/# /' "${lsqa_quality_err_file}"
        else
            printf '# %s: lsqa produced no diagnostics\n' \
                "${lsqa_quality_label}"
        fi
    fi

    rm -f "${lsqa_quality_err_file}"
    return ${lsqa_quality_run_status}
}

lsqa_sixel_init() {
    test_path=$1

    if ! lsqa_init "${test_path}"; then
        return 1
    fi

    _lsqa_require_converter_common
    ensure_converter_available "IMG2SIXEL" "${IMG2SIXEL_PATH}" "img2sixel"

    return 0
}

lsqa_expect_low_quality_or_fail() {
    lsqa_low_image_path=$1
    lsqa_low_label=$2
    lsqa_low_artifact_dir=$3

    lsqa_low_err_file=$(mktemp)
    lsqa_low_run_status=0

    run_lsqa -b "MS-SSIM:0.5" "${lsqa_low_image_path}" \
        "${lsqa_low_image_path}" > /dev/null 2>"${lsqa_low_err_file}" \
        || lsqa_low_run_status=$?

    if [ ${lsqa_low_run_status} -eq 0 ]; then
        printf '# %s: low-quality input accepted\n' \
            "${lsqa_low_label}"
        rm -f "${lsqa_low_err_file}"
        return 1
    fi

    if [ -s "${lsqa_low_err_file}" ]; then
        printf '# %s: assessment/lsqa returned %s\n' \
            "${lsqa_low_label}" "${lsqa_low_run_status}"
        printf '# lsqa stderr follows\n'
        sed 's/^/# /' "${lsqa_low_err_file}"
    else
        printf '# %s: lsqa produced no diagnostics\n' \
            "${lsqa_low_label}"
    fi
    rm -f "${lsqa_low_err_file}"
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
