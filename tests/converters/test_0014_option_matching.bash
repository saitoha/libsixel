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
# shellcheck source=tests/converters/common.bash
source "${SCRIPT_DIR}/common.bash"

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
    'Please choose one of: sierra1, sierra2, sierra3.' \
    -d sie "${image}"

correction_err="${TMP_DIR}/distance1_single.err"
correction_out="${TMP_DIR}/distance1_single.sixel"
rm -f "${correction_err}" "${correction_out}"
if ! run_img2sixel -d burkez "${image}" >"${correction_out}" 2>"${correction_err}"; then
    echo 'expected auto-correction success' >&2
    exit 1
fi
if ! grep -F 'corrected --diffusion value "burkez" -> "burkes".' \
        "${correction_err}" >/dev/null 2>&1; then
    echo 'missing correction notice' >&2
    cat "${correction_err}" >&2
    exit 1
fi

expect_failure "distance1_multi" \
    'Please consider these nearby values: hanning, hamming.' \
    -r hamning "${image}"

expect_failure "distance2" \
    'Please consider these nearby values: hanning, hamming.' \
    -r hamnimg "${image}"

expect_failure "distance3" \
    'specified desampling method is not supported.' \
    -r zzzzz "${image}"
