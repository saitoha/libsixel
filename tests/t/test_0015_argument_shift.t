#!/bin/sh
# TAP test verifying option argument shifting and filenames that resemble flags.

set -eu

test_name=$(basename "$0")
artifact_root=${ARTIFACT_ROOT:-"$(pwd)/_artifacts"}
artifact_dir="${artifact_root}/${test_name}"
log_file="${artifact_dir}/argument-shift.log"
tmp_dir="${artifact_dir}/tmp"

mkdir -p "${artifact_dir}" "${tmp_dir}"

script_dir=$(CDPATH=; cd "$(dirname "$0")" && pwd)
. "${script_dir}/converters-common.t"

status=0
case_id=1

ensure_converter_available "IMG2SIXEL" "${IMG2SIXEL_PATH}" "img2sixel"

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

abs_top=""
if abs_top=$(cd "${top_srcdir}" && pwd); then
    :
else
    echo "failed to resolve source root" >&2
    exit 1
fi

image_path="${abs_top}/images/snake.jpg"
require_file "${image_path}"

echo "1..3"

missing_map_err="${artifact_dir}/missing-map.err"
missing_map_out="${artifact_dir}/missing-map.out"
cleanup_artifacts "${missing_map_err}" "${missing_map_out}"
if run_img2sixel -m -w 100 -h 100 "${image_path}" \
        >"${missing_map_out}" 2>"${missing_map_err}"; then
    fail ${case_id} "accepted -m without argument"
else
    if grep -q 'missing required argument for -m,--mapfile option' \
            "${missing_map_err}"; then
        pass ${case_id} "reported missing mapfile argument"
    else
        fail ${case_id} "no diagnostic for missing -m argument"
        printf '--- stderr ---\n' >>"${log_file}"
        cat "${missing_map_err}" >>"${log_file}" 2>/dev/null || :
    fi
fi
case_id=$((case_id + 1))

outfile_err="${artifact_dir}/outfile-option-name.err"
cleanup_artifacts "${outfile_err}"
rm -f "${tmp_dir}/-p"
if (cd "${tmp_dir}" && \
        run_img2sixel -o -p "${image_path}" \
        >/dev/null 2>"${outfile_err}"); then
    if [ -s "${tmp_dir}/-p" ]; then
        pass ${case_id} "outfile named like option is supported"
    else
        fail ${case_id} "outfile named like option missing"
    fi
else
    fail ${case_id} "outfile named like option rejected"
    printf '--- stderr ---\n' >>"${log_file}"
    cat "${outfile_err}" >>"${log_file}" 2>/dev/null || :
fi
rm -f "${tmp_dir}/-p"
case_id=$((case_id + 1))

assessment_err="${artifact_dir}/assessment-option-name.err"
assessment_json="${artifact_dir}/assessment-option-name.json"
cleanup_artifacts "${assessment_err}" "${assessment_json}"
rm -f "${tmp_dir}/-p"
if (cd "${tmp_dir}" && \
        run_img2sixel -a basic -J -p "${image_path}" \
        >"${assessment_json}" 2>"${assessment_err}"); then
    if [ -s "${tmp_dir}/-p" ]; then
        pass ${case_id} "assessment file named like option is supported"
    else
        fail ${case_id} "assessment file named like option missing"
    fi
else
    fail ${case_id} "assessment file named like option rejected"
    printf '--- stderr ---\n' >>"${log_file}"
    cat "${assessment_err}" >>"${log_file}" 2>/dev/null || :
fi
rm -f "${tmp_dir}/-p"

exit "${status}"
