#!/bin/sh
# TAP test verifying assessment output named like an option is accepted.

set -euxv

script_dir=$(CDPATH=; cd "$(dirname "$0")" && pwd)
. "${script_dir}/../../cli-core/t/cli_core_common.sh"
cli_core_setup "img2sixel-argument-shift"

ensure_converter_available "IMG2SIXEL" "${IMG2SIXEL_PATH}" "img2sixel"

abs_top=""
if abs_top=$(cd "${top_srcdir}" && pwd); then
    :
else
    echo "failed to resolve source root" >&2
    exit 1
fi

image_path="${abs_top}/images/snake.jpg"
require_file "${image_path}"

echo "1..1"

assessment_err="${artifact_dir}/assessment-option-name.err"
assessment_json="${artifact_dir}/assessment-option-name.json"
rm -f "${assessment_err}" "${assessment_json}" "${tmp_dir}/-p"

if (cd "${tmp_dir}" && \
        run_img2sixel -a basic -J -p "${image_path}" \
        >"${assessment_json}" 2>"${assessment_err}"); then
    if [ -s "${tmp_dir}/-p" ]; then
        cli_core_pass 1 "assessment file named like option is supported"
    else
        cli_core_fail 1 "assessment file named like option missing"
    fi
else
    cli_core_fail 1 "assessment file named like option rejected"
    printf '--- stderr ---\n' >>"${log_file}"
    cat "${assessment_err}" >>"${log_file}" 2>/dev/null || :
fi

rm -f "${tmp_dir}/-p"

exit "${status}"
