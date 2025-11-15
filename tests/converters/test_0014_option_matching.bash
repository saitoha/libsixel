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
if ! grep -F 'Did you mean:' "${TMP_DIR}/distance1_multi.err" \
        >/dev/null 2>&1; then
    echo 'missing suggestion banner for distance1_multi' >&2
    cat "${TMP_DIR}/distance1_multi.err" >&2
    exit 1
fi
if ! grep -F 'hanning' "${TMP_DIR}/distance1_multi.err" >/dev/null 2>&1; then
    echo 'missing hanning suggestion for distance1_multi' >&2
    cat "${TMP_DIR}/distance1_multi.err" >&2
    exit 1
fi
if ! grep -F 'hamming' "${TMP_DIR}/distance1_multi.err" >/dev/null 2>&1; then
    echo 'missing hamming suggestion for distance1_multi' >&2
    cat "${TMP_DIR}/distance1_multi.err" >&2
    exit 1
fi

expect_failure "prefix_projection" \
    'specified diffusion method is not supported.' \
    -d ato "${image}"
if ! grep -F 'Did you mean:' "${TMP_DIR}/prefix_projection.err" \
        >/dev/null 2>&1; then
    echo 'missing suggestion banner for prefix_projection' >&2
    cat "${TMP_DIR}/prefix_projection.err" >&2
    exit 1
fi
if ! grep -F 'auto' "${TMP_DIR}/prefix_projection.err" >/dev/null 2>&1; then
    echo 'missing auto suggestion for prefix_projection' >&2
    cat "${TMP_DIR}/prefix_projection.err" >&2
    exit 1
fi
if ! grep -F 'atkinson' "${TMP_DIR}/prefix_projection.err" >/dev/null 2>&1; then
    echo 'missing atkinson suggestion for prefix_projection' >&2
    cat "${TMP_DIR}/prefix_projection.err" >&2
    exit 1
fi

expect_failure "distance2" \
    'specified desampling method is not supported.' \
    -r hamnimg "${image}"
if ! grep -F 'Did you mean:' "${TMP_DIR}/distance2.err" \
        >/dev/null 2>&1; then
    echo 'missing suggestion banner for distance2' >&2
    cat "${TMP_DIR}/distance2.err" >&2
    exit 1
fi

expect_failure "distance3" \
    'specified desampling method is not supported.' \
    -r zzzzz "${image}"
if grep -F 'Did you mean:' "${TMP_DIR}/distance3.err" >/dev/null 2>&1; then
    echo 'unexpected suggestion for distance3' >&2
    cat "${TMP_DIR}/distance3.err" >&2
    exit 1
fi

env_distance2_err="${TMP_DIR}/env_distance2.err"
env_distance2_out="${TMP_DIR}/env_distance2.sixel"
rm -f "${env_distance2_err}" "${env_distance2_out}"
if SIXEL_OPTION_FUZZY_SUGGESTIONS=0 \
        run_img2sixel -r hamnimg "${image}" \
            >"${env_distance2_out}" 2>"${env_distance2_err}"; then
    echo 'expected env_distance2 failure' >&2
    exit 1
fi
if grep -F 'Did you mean:' "${env_distance2_err}" >/dev/null 2>&1; then
    echo 'unexpected fuzzy suggestion when disabled' >&2
    cat "${env_distance2_err}" >&2
    exit 1
fi
if ! grep -F 'specified desampling method is not supported.' \
        "${env_distance2_err}" >/dev/null 2>&1; then
    echo 'missing base error for env_distance2' >&2
    cat "${env_distance2_err}" >&2
    exit 1
fi

env_prefix_err="${TMP_DIR}/env_prefix.err"
env_prefix_out="${TMP_DIR}/env_prefix.sixel"
rm -f "${env_prefix_err}" "${env_prefix_out}"
if SIXEL_OPTION_PREFIX_SUGGESTIONS=0 \
        run_img2sixel -d sie "${image}" \
            >"${env_prefix_out}" 2>"${env_prefix_err}"; then
    echo 'expected env_prefix failure' >&2
    exit 1
fi
if grep -F '(matches:' "${env_prefix_err}" >/dev/null 2>&1; then
    echo 'unexpected prefix suggestion when disabled' >&2
    cat "${env_prefix_err}" >&2
    exit 1
fi
if ! grep -F 'ambiguous prefix "sie"' "${env_prefix_err}" >/dev/null 2>&1; then
    echo 'missing base prefix error for env_prefix' >&2
    cat "${env_prefix_err}" >&2
    exit 1
fi

env_path_err="${TMP_DIR}/env_path.err"
env_path_out="${TMP_DIR}/env_path.sixel"
rm -f "${env_path_err}" "${env_path_out}"
if SIXEL_OPTION_PATH_SUGGESTIONS=0 \
        run_img2sixel "${TMP_DIR}/does-not-exist.png" \
            >"${env_path_out}" 2>"${env_path_err}"; then
    echo 'expected env_path failure' >&2
    exit 1
fi
if grep -F 'Suggestions:' "${env_path_err}" >/dev/null 2>&1; then
    echo 'unexpected path suggestion when disabled' >&2
    cat "${env_path_err}" >&2
    exit 1
fi
if ! grep -F 'path "' "${env_path_err}" >/dev/null 2>&1; then
    echo 'missing base path error for env_path' >&2
    cat "${env_path_err}" >&2
    exit 1
fi
