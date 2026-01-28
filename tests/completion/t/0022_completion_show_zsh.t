#!/bin/sh
# TAP test verifying zsh completion output from img2sixel.

set -eux

test_name=$(basename "$0")
test_dir=$(CDPATH=; cd "$(dirname "$0")" && pwd)
category_name=$(basename "$(dirname "${test_dir}")")
artifact_root=${ARTIFACT_ROOT:-"$(pwd)/_artifacts"}
artifact_dir="${artifact_root}/${category_name}/${test_name}"
log_file="${artifact_dir}/completion.log"
output_file="${artifact_dir}/completion.zsh"

mkdir -p "${artifact_dir}"

script_dir=$(CDPATH=; cd "$(dirname "$0")" && pwd)
. "${script_dir}/../../common/t/0001_converters_common.t"

status=0

ensure_converter_available "IMG2SIXEL" "${IMG2SIXEL_PATH}" "img2sixel"



completion_dir="${top_srcdir}/converters/shell-completion"
completion_source="${completion_dir}/zsh/_img2sixel"
require_file "${completion_source}"

printf '1..1\n'
set -v

IMG2SIXEL_COMPLETION_DIR="${completion_dir}"
export IMG2SIXEL_COMPLETION_DIR

if run_img2sixel -1 zsh >"${output_file}" 2>>"${log_file}"; then
    if grep -F '#compdef img2sixel' "${output_file}" >/dev/null 2>&1; then
        pass 1 "zsh completion output available"
    else
        fail 1 "missing zsh completion header"
    fi
else
    fail 1 "zsh completion output failed"
fi

exit "${status}"
