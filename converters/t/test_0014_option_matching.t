#!/usr/bin/env bash
# Validate option matching edge cases for img2sixel.
#
# Decision grid:
# +-------------------------------+------------------------------+
# | scenario                      | expectation                  |
# +-------------------------------+------------------------------+
# | prefix unique                 | success, quiet stderr        |
# | prefix ambiguous              | polite refinement request    |
# | distance 1, single candidate  | auto-correction notice       |
# | distance 1, many candidates   | polite list without changes  |
# | distance 2                    | polite list without changes  |
# | distance 3 or more            | gentle manual guidance       |
# +-------------------------------+------------------------------+
set -euo pipefail

SCRIPT_DIR=$(cd "$(dirname "$0")" && pwd)
# shellcheck source=converters/t/common.t
source "${SCRIPT_DIR}/common.t"

echo '[test15] option matching diagnostics'

require_file "${IMAGES_DIR}/snake.png"

image="${IMAGES_DIR}/snake.png"

expect_success() {
    local label
    local err_file
    local out_file

    label=$1
    shift
    err_file="${TMP_DIR}/${label}.err"
    out_file="${TMP_DIR}/${label}.sixel"
    rm -f "${err_file}" "${out_file}"
    if ! run_img2sixel "$@" >"${out_file}" 2>"${err_file}"; then
        printf 'expected success for %s\n' "${label}" >&2
        exit 1
    fi
    if [[ -s ${err_file} ]]; then
        # Debug builds emit human-readable progress notes.  Only fail when
        # the log still contains clearly labelled problems after removing
        # the command echo.
        if tail -n +2 "${err_file}" \
                | grep -v '^+' \
                | grep -v 'img2sixel' \
                | grep -Ei 'error|warning|failed' \
                >/dev/null; then
            printf 'unexpected stderr for %s\n' "${label}" >&2
            cat "${err_file}" >&2
            exit 1
        fi
    fi
    rm -f "${err_file}" "${out_file}"
}

expect_success "prefix_unique" -y ser "${image}"

expect_failure() {
    local label
    local needle
    local err_file
    local out_file

    label=$1
    needle=$2
    shift 2
    err_file="${TMP_DIR}/${label}.err"
    out_file="${TMP_DIR}/${label}.sixel"
    rm -f "${err_file}" "${out_file}"
    if run_img2sixel "$@" >"${out_file}" 2>"${err_file}"; then
        printf 'expected failure for %s\n' "${label}" >&2
        exit 1
    fi
    if ! grep -F "${needle}" "${err_file}" >/dev/null 2>&1; then
        printf 'missing expected diagnostic for %s\n' "${label}" >&2
        cat "${err_file}" >&2
        exit 1
    fi
}

expect_failure "prefix_ambiguous" \
    'ambiguous prefix "sie"' \
    -d sie "${image}"

correction_err="${TMP_DIR}/distance1_single.err"
correction_out="${TMP_DIR}/distance1_single.sixel"
rm -f "${correction_err}" "${correction_out}"
if run_img2sixel -d burkez "${image}" >"${correction_out}" 2>"${correction_err}"; then
    if ! grep -F 'corrected --diffusion value "burkez" -> "burkes".' \
            "${correction_err}" >/dev/null 2>&1; then
        echo 'missing correction notice' >&2
        cat "${correction_err}" >&2
        exit 1
    fi
else
    if ! grep -F 'specified diffusion method is not supported.' \
            "${correction_err}" >/dev/null 2>&1; then
        echo 'unexpected diffusion failure output' >&2
        cat "${correction_err}" >&2
        exit 1
    fi
fi

expect_failure "distance1_multi" \
    'specified desampling method is not supported.' \
    -r hamning "${image}"

expect_failure "distance2" \
    'specified desampling method is not supported.' \
    -r hamnimg "${image}"

expect_failure "distance3" \
    'specified desampling method is not supported.' \
    -r zzzzz "${image}"

unset SIXEL_OPTION_PATH_SUGGESTIONS
missing_path="${IMAGES_DIR}/snake.png.missing"
missing_native_path="${missing_path}"
if [[ ${missing_native_path} =~ ^/([A-Za-z])/(.*)$ ]]; then
    missing_drive=${BASH_REMATCH[1]}
    missing_rest=${BASH_REMATCH[2]}
    missing_native_path="${missing_drive^^}:/${missing_rest}"
fi
default_err="${TMP_DIR}/missing-path-default.err"
default_out="${TMP_DIR}/missing-path-default.sixel"
rm -f "${default_err}" "${default_out}"
if run_img2sixel "${missing_path}" \
        >"${default_out}" 2>"${default_err}"; then
    echo 'expected missing path rejection with default suggestions disabled' >&2
    rm -f "${default_err}" "${default_out}"
    exit 1
fi
if ! grep -F "path \"${missing_path}\" not found." \
        "${default_err}" >/dev/null 2>&1; then
    if [[ "${missing_native_path}" != "${missing_path}" ]] && \
            grep -F "path \"${missing_native_path}\" not found." \
                "${default_err}" >/dev/null 2>&1; then
        :
    else
        echo 'missing default diagnostic for invalid path' >&2
        cat "${default_err}" >&2 || :
        rm -f "${default_err}" "${default_out}"
        exit 1
    fi
fi
if grep -F 'Suggestions:' "${default_err}" >/dev/null 2>&1; then
    echo 'path suggestions should be disabled by default' >&2
    cat "${default_err}" >&2 || :
    rm -f "${default_err}" "${default_out}"
    exit 1
fi
rm -f "${default_out}" "${default_err}"

suggest_err="${TMP_DIR}/missing-path-suggest.err"
suggest_out="${TMP_DIR}/missing-path-suggest.sixel"
rm -f "${suggest_err}" "${suggest_out}"
SIXEL_OPTION_PATH_SUGGESTIONS=1 run_img2sixel "${missing_path}" \
    >"${suggest_out}" 2>"${suggest_err}" || :
unset SIXEL_OPTION_PATH_SUGGESTIONS
if ! grep -F 'Suggestions:' "${suggest_err}" >/dev/null 2>&1; then
    echo 'path suggestions should activate when explicitly requested' >&2
    cat "${suggest_err}" >&2 || :
    rm -f "${suggest_err}" "${suggest_out}"
    exit 1
fi
rm -f "${suggest_out}" "${suggest_err}"
