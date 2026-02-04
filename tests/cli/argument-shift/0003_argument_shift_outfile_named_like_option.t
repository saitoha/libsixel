#!/bin/sh
# TAP test verifying output file named like an option is accepted.

set -eux

script_dir=$(CDPATH=; cd "$(dirname "$0")" && pwd)
CLI_CORE_HELPER_DIR="${TOP_SRCDIR}/tests/lib/sh/cli-core"
. "${CLI_CORE_HELPER_DIR}/cli_core_common.sh"
cli_core_setup "img2sixel-argument-shift"

ensure_converter_available "IMG2SIXEL" "${IMG2SIXEL_PATH}" "img2sixel"

image_path="${top_srcdir}/tests/data/inputs/snake_64.jpg"
require_file "${image_path}"

echo "1..1"
set -v

outfile_err="${artifact_dir}/outfile-option-name.err"
rm -f "${outfile_err}" "${tmp_dir}/-p"

if (cd "${tmp_dir}" && run_img2sixel -o -p "${image_path}" >/dev/null 2>"${outfile_err}"); then
    if [ -s "${tmp_dir}/-p" ]; then
        cli_core_pass 1 "outfile named like option is supported"
    else
        cli_core_fail 1 "outfile named like option missing"
    fi
else
    cli_core_fail 1 "outfile named like option rejected"
    printf '%s\n' '--- stderr ---' >>"${log_file}"
    cat "${outfile_err}" >>"${log_file}" 2>/dev/null || :
fi

rm -f "${tmp_dir}/-p"

exit "${status}"
