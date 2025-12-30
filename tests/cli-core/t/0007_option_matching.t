#!/bin/sh
# TAP test validating img2sixel option matching diagnostics.

# Enable strict mode with verbose tracing for diagnostics.
set -euxv

test_name=$(basename "$0")
artifact_root=${ARTIFACT_ROOT:-"$(pwd)/_artifacts"}
artifact_dir="${artifact_root}/${test_name}"
log_file="${artifact_dir}/option-matching.log"

mkdir -p "${artifact_dir}"

script_dir=$(CDPATH=; cd "$(dirname "$0")" && pwd)
. "${script_dir}/../../common/t/0001_converters_common.t"

ensure_converter_available "IMG2SIXEL" "${IMG2SIXEL_PATH}" "img2sixel"

require_file "${images_dir}/snake.png"

status=0
case_id=1

pass() {
    printf 'ok %s - %s\n' "$1" "$2"
}

fail() {
    printf 'not ok %s - %s\n' "$1" "$2"
    status=1
}

cleanup_artifacts() {
    rm -f "$@" || :
}

expect_success() {
    label=$1
    shift
    err_file="${artifact_dir}/${label}.err"
    out_file="${artifact_dir}/${label}.sixel"

    cleanup_artifacts "${err_file}" "${out_file}"

    if run_img2sixel "$@" >"${out_file}" 2>"${err_file}"; then
        :
    else
        fail ${case_id} "${label} unexpectedly failed"
        return
    fi

    if [ -s "${err_file}" ]; then
        filtered_err="${artifact_dir}/${label}.filtered.err"
        if sed '1d' "${err_file}" \
                | grep -v '^+' \
                | grep -v 'img2sixel' \
                | grep -Ei 'error|warning|failed' \
                >"${filtered_err}"; then
            if [ -s "${filtered_err}" ]; then
                fail ${case_id} "${label} emitted diagnostics"
                printf '--- stderr ---\n' >>"${log_file}"
                cat "${err_file}" >>"${log_file}" 2>/dev/null || :
                cleanup_artifacts "${filtered_err}"
                return
            fi
        fi
        cleanup_artifacts "${filtered_err}"
    fi

    pass ${case_id} "${label} succeeded"
}

expect_failure() {
    label=$1
    needle=$2
    shift 2
    err_file="${artifact_dir}/${label}.err"
    out_file="${artifact_dir}/${label}.sixel"

    cleanup_artifacts "${err_file}" "${out_file}"

    if run_img2sixel "$@" >"${out_file}" 2>"${err_file}"; then
        fail ${case_id} "${label} unexpectedly succeeded"
        return
    fi

    if grep -F "${needle}" "${err_file}" >/dev/null 2>&1; then
        pass ${case_id} "${label} reported expected diagnostic"
    else
        fail ${case_id} "${label} missing diagnostic"
        printf '--- stderr ---\n' >>"${log_file}"
        cat "${err_file}" >>"${log_file}" 2>/dev/null || :
    fi
}

echo "1..6"

expect_success "prefix_unique" -y ser "${images_dir}/snake.png"
case_id=$((case_id + 1))

expect_failure "prefix_ambiguous" "ambiguous prefix \"sie\"" \
    -d sie "${images_dir}/snake.png"
case_id=$((case_id + 1))

correction_err="${artifact_dir}/distance1_single.err"
correction_out="${artifact_dir}/distance1_single.sixel"
cleanup_artifacts "${correction_err}" "${correction_out}"
if run_img2sixel -d burkez "${images_dir}/snake.png" \
        >"${correction_out}" 2>"${correction_err}"; then
    if grep -F 'corrected --diffusion value "burkez" -> "burkes".' \
            "${correction_err}" >/dev/null 2>&1; then
        pass ${case_id} "distance1 single candidate corrected"
    else
        fail ${case_id} "missing correction notice"
        printf '--- stderr ---\n' >>"${log_file}"
        cat "${correction_err}" >>"${log_file}" 2>/dev/null || :
    fi
else
    if grep -F 'specified diffusion method is not supported.' \
            "${correction_err}" >/dev/null 2>&1; then
        pass ${case_id} "distance1 single candidate rejected"
    else
        fail ${case_id} "unexpected diffusion rejection"
        printf '--- stderr ---\n' >>"${log_file}"
        cat "${correction_err}" >>"${log_file}" 2>/dev/null || :
    fi
fi
case_id=$((case_id + 1))

expect_failure "distance1_multi" \
    'specified desampling method is not supported.' \
    -r hamning "${images_dir}/snake.png"
case_id=$((case_id + 1))

expect_failure "distance2" \
    'specified desampling method is not supported.' \
    -r hamnimg "${images_dir}/snake.png"
case_id=$((case_id + 1))

expect_failure "distance3" \
    'specified desampling method is not supported.' \
    -r zzzzz "${images_dir}/snake.png"

exit "${status}"
