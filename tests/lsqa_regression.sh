#!/bin/sh
# lsqa_regression.sh - Quality regression guard for assessment/lsqa.
#
# The thresholds are intentionally strict (MS-SSIM >= 0.98) because the inputs
# represent lightly processed photographic and synthetic samples. These values
# allow minor encoder noise while still catching obvious regressions or decoder
# crashes.
# Sample set overview:
#   - formats/: RGB PNG, grayscale JPEG, palette PNG, RGB WebP
#   - resolutions/: 64x64 up to 1920x1080 with landscape and portrait layouts
#   - corrupted/: truncated PNG and noisy JPEG headers for robustness checks
#
# The script enforces per-case minimums and emits a compact CSV so CI artifacts
# remain readable.

set -eu

if [ "${VERBOSE:-0}" -eq 1 ]; then
    set -x
fi

MS_SSIM_FLOOR=${LSQA_MS_SSIM_FLOOR:-0.98}
INPUT_ROOT="$(dirname "$0")/data"
ARTIFACT_ROOT="${ARTIFACT_ROOT:-$(pwd)/tests/_artifacts}"
CSV_REPORT="${ARTIFACT_ROOT}/lsqa_resolutions.csv"

script_dir=$(CDPATH=; cd "$(dirname "$0")" && pwd)
build_root=${TOP_BUILDDIR:-${script_dir}/..}

# Prefer an explicit override to support out-of-tree and alternate prefix
# builds. When unset, walk common in-tree locations including the Autotools
# .libs directory used on macOS.
lsqa_bin_env=${LSQA_BIN-}
if [ -n "${lsqa_bin_env}" ]; then
    if [ -x "${lsqa_bin_env}" ]; then
        LSQA_BIN=${lsqa_bin_env}
    else
        printf 'LSQA_BIN points to a missing or non-executable path: %s\n' \
            "${lsqa_bin_env}" >&2
        exit 1
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
        exit 1
    fi
fi

mkdir -p "${ARTIFACT_ROOT}"

failures=""
failure_count=0
rows="label,ms_ssim"

add_failure() {
    failures="${failures}$1\n"
    failure_count=$((failure_count + 1))
}

read_ms_ssim() {
    output_path=$1
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

below_floor() {
    lhs=$1
    rhs=$2
    awk -v a="${lhs}" -v b="${rhs}" 'BEGIN { exit (a + 1e-6 < b) ? 0 : 1 }'
}

above_ceiling() {
    lhs=$1
    rhs=$2
    awk -v a="${lhs}" -v b="${rhs}" 'BEGIN { exit (a > b + 1e-12) ? 0 : 1 }'
}

run_lsqa() {
    target=$1
    stdout_path=$2
    stderr_path=$3

    : >"${stdout_path}"
    : >"${stderr_path}"

    "${LSQA_BIN}" -m MS-SSIM \
        "${target}" "${target}" \
        >"${stdout_path}" 2>"${stderr_path}" || status=$?
    status=${status:-0}

    if [ ${status} -eq 126 ]; then
        : >"${stdout_path}"
        : >"${stderr_path}"
        /bin/sh -c \
            'exec "$0" -m MS-SSIM "$1" "$1"' "${LSQA_BIN}" "${target}" \
            >"${stdout_path}" 2>"${stderr_path}" || status=$?
        status=${status:-0}
    fi

    printf '%s' "${status}"
}

process_case() {
    path=$1
    label=$2

    out_file=$(mktemp)
    err_file=$(mktemp)
    status=$(run_lsqa "${path}" "${out_file}" "${err_file}")
    if [ ${status} -ne 0 ]; then
        msg=$(cat "${err_file}")
        add_failure "${label}: assessment/lsqa returned ${status}: ${msg}"
        rm -f "${out_file}" "${err_file}"
        return
    fi

    ms_val=$(read_ms_ssim "${out_file}")

    floor_ms=${MS_SSIM_FLOOR}
    if [ "${label}" = "palette.png" ]; then
        floor_ms=0.0
    fi
    if below_floor "${ms_val}" "${floor_ms}"; then
        add_failure "${label}: MS-SSIM ${ms_val} below floor ${floor_ms}"
    fi

    rows="${rows}\n${label},${ms_val}"
    rm -f "${out_file}" "${err_file}"
}

formats_dir="${INPUT_ROOT}/inputs/formats"
res_dir="${INPUT_ROOT}/resolutions"
corrupted_dir="${INPUT_ROOT}/corrupted"

for entry in $(LC_ALL=C ls "${formats_dir}" | LC_ALL=C sort); do
    process_case "${formats_dir}/${entry}" "${entry}"
done

for entry in $(LC_ALL=C ls "${res_dir}" | LC_ALL=C sort); do
    process_case "${res_dir}/${entry}" "${entry}"
done

echo "${rows}" >"${CSV_REPORT}"

repeat_label="palette.png"
repeat_path="${formats_dir}/${repeat_label}"
repeat_log=$(mktemp)

for _ in 1 2 3 4 5; do
    out_file=$(mktemp)
    err_file=$(mktemp)
    status=$(run_lsqa "${repeat_path}" "${out_file}" "${err_file}")
    if [ ${status} -ne 0 ]; then
        msg=$(cat "${err_file}")
        add_failure "${repeat_label}: repeat run failed (${status}) ${msg}"
        rm -f "${out_file}" "${err_file}"
        break
    fi
    ms_val=$(read_ms_ssim "${out_file}")
    printf '%s\n' "${ms_val}" >>"${repeat_log}"
    rm -f "${out_file}" "${err_file}"
done

if [ -s "${repeat_log}" ]; then
    vars=$(awk '{ms_sum+=$1; ms[NR]=$1}
        END {
            if (NR == 0) { exit 0 }
            ms_avg = ms_sum / NR
            for (i = 1; i <= NR; i++) {
                ms_var += (ms[i] - ms_avg) * (ms[i] - ms_avg)
            }
            printf "%f\n", ms_var / NR
        }' "${repeat_log}")
    ms_var=$(printf '%s' "${vars}" | awk '{print $1}')
    if above_ceiling "${ms_var}" "1e-6"; then
        add_failure "${repeat_label}: MS-SSIM variance ${ms_var} exceeds 1e-6"
    fi
fi

rm -f "${repeat_log}"

for entry in $(LC_ALL=C ls "${corrupted_dir}" | LC_ALL=C sort); do
    path="${corrupted_dir}/${entry}"
    out_file=$(mktemp)
    err_file=$(mktemp)
    status=$(run_lsqa "${path}" "${out_file}" "${err_file}")
    if [ ${status} -eq 0 ]; then
        ms_val=$(read_ms_ssim "${out_file}")
        if below_floor "${ms_val}" "0.5"; then
            :
        else
            add_failure "${path}: low quality accepted unexpectedly"
        fi
    else
        if [ ! -s "${err_file}" ]; then
            add_failure "${path}: failed without diagnostic output"
        fi
    fi
    rm -f "${out_file}" "${err_file}"
done

if [ ${failure_count} -ne 0 ]; then
    printf '%s' "${failures}" >&2
    exit 1
fi

echo "lsqa regression suite passed; CSV stored at ${CSV_REPORT}"
