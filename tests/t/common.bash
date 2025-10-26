#!/usr/bin/env bash
# Shared helpers for converter tests.
set -euo pipefail

normalize_path() {
    local path
    local drive
    local rest

    path=$1

    if [[ ${path} =~ ^\\\\ ]]; then
        rest=${path#\\\\}
        rest=${rest//\\//}
        printf '//%s' "${rest}"
        return 0
    fi

    if [[ ${path} =~ ^[A-Za-z]:[\\/].* ]]; then
        if command -v cygpath >/dev/null 2>&1; then
            cygpath -u "${path}"
            return 0
        fi
        drive=${path:0:1}
        rest=${path:2}
        rest=${rest//\\//}
        rest=${rest#/}
        drive=${drive,,}
        if [[ -n ${rest} ]]; then
            printf '/%s/%s' "${drive}" "${rest}"
        else
            printf '/%s' "${drive}"
        fi
        return 0
    fi

    printf '%s' "${path}"
    return 0
}

if [[ -z "${TOP_SRCDIR:-}" ]]; then
    echo "TOP_SRCDIR is not set" >&2
    exit 1
fi
if [[ -z "${SRC_DIR:-}" ]]; then
    echo "SRC_DIR is not set" >&2
    exit 1
fi
if [[ -z "${BUILD_DIR:-}" ]]; then
    echo "BUILD_DIR is not set" >&2
    exit 1
fi

TOP_SRCDIR=$(normalize_path "${TOP_SRCDIR}")
SRC_DIR=$(normalize_path "${SRC_DIR}")
BUILD_DIR=$(normalize_path "${BUILD_DIR}")
BUILD_DIR_ABS=$(cd "${BUILD_DIR}" && pwd)
TMP_DIR=${TMP_DIR:-"${BUILD_DIR}/tmp"}
TMP_DIR=$(normalize_path "${TMP_DIR}")
if [[ ${TMP_DIR} != /* ]]; then
    TMP_DIR="${BUILD_DIR_ABS}/${TMP_DIR}"
fi
WINE=${WINE:-}
WINEEXT=${WINEEXT:-}

resolve_converter_executable() {
    local -a search_paths
    local candidate

    search_paths=("$@")

    for candidate in "${search_paths[@]}"; do
        if [[ -x "${candidate}" ]]; then
            printf '%s\n' "${candidate}"
            return 0
        fi
    done

    printf '%s\n' "${search_paths[0]}"
    return 1
}

IMG2SIXEL_CANDIDATES=(
    "${BUILD_DIR_ABS}/img2sixel${WINEEXT}"
    "${BUILD_DIR_ABS}/converters/img2sixel${WINEEXT}"
)
SIXEL2PNG_CANDIDATES=(
    "${BUILD_DIR_ABS}/sixel2png${WINEEXT}"
    "${BUILD_DIR_ABS}/converters/sixel2png${WINEEXT}"
)

IMG2SIXEL_PATH=$(resolve_converter_executable \
    "${IMG2SIXEL_CANDIDATES[@]}")
if [[ ! -x "${IMG2SIXEL_PATH}" ]]; then
    printf 'warning: img2sixel candidates missing (%s)\n' \
        "$(printf '%s ' "${IMG2SIXEL_CANDIDATES[@]}")" >&2
fi

SIXEL2PNG_PATH=$(resolve_converter_executable \
    "${SIXEL2PNG_CANDIDATES[@]}")
if [[ ! -x "${SIXEL2PNG_PATH}" ]]; then
    printf 'warning: sixel2png candidates missing (%s)\n' \
        "$(printf '%s ' "${SIXEL2PNG_CANDIDATES[@]}")" >&2
fi

IMAGES_DIR="${TOP_SRCDIR}/images"

mkdir -p "${TMP_DIR}"

tap_init() {
    local base
    local log_dir

    base=$1
    log_dir="${TMP_DIR}/logs"
    mkdir -p "${log_dir}"
    TAP_LOG_FILE="${log_dir}/${base}.log"
    : >"${TAP_LOG_FILE}"
    TAP_CASE_INDEX=0
}

tap_plan() {
    local total

    total=$1
    printf '1..%s\n' "${total}"
}

# ----------------------------------------------------------------------
#  +----------------------+
#  | TAP case navigation |
#  +----------------------+
#  The helper below hands out monotonically increasing identifiers so
#  that each test case can simply call `tap_next` before emitting
#  success or failure.  The ASCII art illustrates the very small state
#  machine we maintain:
#
#        +---------+   tap_next   +---------+
#        |  idle   | -----------> | running |
#        +---------+              +---------+
#            ^                         |
#            |   tap_init              |
#            +-------------------------+
#
#  The implementation is intentionally tiny yet explicit because Bash
#  does not have integers with automatic declaration.
# ----------------------------------------------------------------------
tap_next() {
    TAP_CASE_INDEX=$((TAP_CASE_INDEX + 1))
}

tap_case() {
    local description
    local callback
    local number

    description=$1
    callback=$2
    shift 2
    tap_next
    number=${TAP_CASE_INDEX}
    if "${callback}" "$@" >>"${TAP_LOG_FILE}" 2>&1; then
        tap_ok "${number}" "${description}"
    else
        tap_not_ok "${number}" "${description}" \
            "See $(tap_log_hint) for details."
    fi
}

tap_diag() {
    local message

    for message in "$@"; do
        printf '# %s\n' "${message}"
    done
}

tap_ok() {
    local number
    local description

    number=$1
    description=$2
    shift 2
    printf 'ok %s - %s\n' "${number}" "${description}"
    if [ $# -gt 0 ]; then
        tap_diag "$@"
    fi
}

tap_not_ok() {
    local number
    local description

    number=$1
    description=$2
    shift 2
    printf 'not ok %s - %s\n' "${number}" "${description}"
    if [ $# -gt 0 ]; then
        tap_diag "$@"
    fi
}

tap_log() {
    local message

    if [ -z "${TAP_LOG_FILE:-}" ]; then
        return 0
    fi
    for message in "$@"; do
        printf '%s\n' "${message}" >>"${TAP_LOG_FILE}"
    done
}

tap_log_hint() {
    local path

    path=${TAP_LOG_FILE:-}
    if [ -z "${path}" ]; then
        return 0
    fi
    if [ -n "${BUILD_DIR_ABS:-}" ] && [[ ${path} == ${BUILD_DIR_ABS}/* ]]; then
        printf '%s\n' "${path#${BUILD_DIR_ABS}/}"
        return 0
    fi
    printf '%s\n' "${path}"
}

# ----------------------------------------------------------------------
#  WINE launcher sanitization pipeline
#
#      +-------------------+     +---------------------------+
#      | $WINE token array |---->| drop trailing "make check" |
#      +-------------------+     +---------------------------+
#                  |                          |
#                  +--------------------------+
#                  |
#                  v
#            sanitized runner
# ----------------------------------------------------------------------
wine_exec() {
    local -a wine_tokens
    local -a sanitized_runner
    local token
    local command_path
    local encountered_make

    wine_tokens=()
    sanitized_runner=()
    encountered_make=0

    if [[ -n "${WINE}" ]]; then
        read -r -a wine_tokens <<<"${WINE}"
        for token in "${wine_tokens[@]}"; do
            if [[ "${token}" == "make" ]]; then
                encountered_make=1
                break
            fi
            sanitized_runner+=("${token}")
        done
        if [[ ${encountered_make} -eq 0 ]]; then
            if [[ ${#sanitized_runner[@]} -gt 0 ]]; then
                command_path="${sanitized_runner[0]}"
                if command -v "${command_path}" >/dev/null 2>&1; then
                    "${sanitized_runner[@]}" "$@"
                    return 0
                fi
            fi
        fi
        # Automake's converter harness sometimes feeds us a synthetic
        # "wine $(MAKE) check" value which is only meant to satisfy the
        # variable dependency graph.  When we detect the stray "make"
        # token we intentionally skip the sanitized runner so that we
        # fall back to native execution even if a Wine binary is present.
    fi

    "$@"
}

run_img2sixel() {
    wine_exec "${IMG2SIXEL_PATH}" "$@"
}

run_sixel2png() {
    wine_exec "${SIXEL2PNG_PATH}" "$@"
}

require_file() {
    if [[ ! -e "$1" ]]; then
        echo "Required file '$1' is missing" >&2
        exit 1
    fi
}
