#!/bin/sh
# TAP test verifying bash completion installation from img2sixel.

set -euxv

test_name=$(basename "$0")
test_dir=$(CDPATH=; cd "$(dirname "$0")" && pwd)
category_name=$(basename "$(dirname "${test_dir}")")
artifact_root=${ARTIFACT_ROOT:-"$(pwd)/_artifacts"}
artifact_dir="${artifact_root}/${category_name}/${test_name}"
log_file="${artifact_dir}/completion.log"

mkdir -p "${artifact_dir}"

script_dir=$(CDPATH=; cd "$(dirname "$0")" && pwd)
. "${script_dir}/../../common/t/0001_converters_common.t"

status=0

ensure_converter_available "IMG2SIXEL" "${IMG2SIXEL_PATH}" "img2sixel"

pass() {
    printf 'ok %s - %s\n' "$1" "$2"
}

fail() {
    printf 'not ok %s - %s\n' "$1" "$2"
    status=1
}

completion_dir="${top_srcdir}/converters/shell-completion"
completion_source="${completion_dir}/bash/img2sixel"
completion_home="${artifact_dir}/home"
target_path="${completion_home}/.local/share/bash-completion/completions/img2sixel"

require_file "${completion_source}"
rm -rf "${completion_home}"
mkdir -p "${completion_home}"

printf '1..1\n'

if IMG2SIXEL_COMPLETION_HOME="${completion_home}" \
        IMG2SIXEL_COMPLETION_DIR="${completion_dir}" \
        BASH_VERSION=5.0 \
        run_img2sixel -2 bash >"${log_file}" 2>&1; then
    if [ -f "${target_path}" ] && \
            grep -F '# bash completion for img2sixel' \
            "${target_path}" >/dev/null 2>&1; then
        pass 1 "bash completion installed"
    else
        fail 1 "bash completion not installed"
    fi
else
    fail 1 "bash completion install failed"
fi

exit "${status}"
