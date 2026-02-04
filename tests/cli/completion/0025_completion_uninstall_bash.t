#!/bin/sh
# TAP test verifying bash completion removal from img2sixel.

set -eux

test_name=$(basename "$0")
test_dir=$(CDPATH=; cd "$(dirname "$0")" && pwd)
category_name=$(basename "$(dirname "${test_dir}")")
artifact_root=${ARTIFACT_ROOT:-"$(pwd)/_artifacts"}
artifact_test_dir=$(dirname "$0")
artifact_dir="${artifact_root}/${artifact_test_dir}/${test_name}"
log_file="${artifact_dir}/completion.log"

mkdir -p "${artifact_dir}"

script_dir=$(CDPATH=; cd "$(dirname "$0")" && pwd)
. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

status=0

ensure_converter_available "IMG2SIXEL" "${IMG2SIXEL_PATH}" "img2sixel"



completion_home="${artifact_dir}/home"
primary_path="${completion_home}/.local/share/bash-completion/completions/img2sixel"
legacy_path="${completion_home}/.bash_completion.d/img2sixel"

rm -rf "${completion_home}"
mkdir -p "$(dirname "${primary_path}")"
mkdir -p "$(dirname "${legacy_path}")"
printf '# stub completion\n' >"${primary_path}"
printf '# stub completion\n' >"${legacy_path}"

printf '1..1\n'
set -v

IMG2SIXEL_COMPLETION_HOME="${completion_home}"
export IMG2SIXEL_COMPLETION_HOME

if run_img2sixel -3 bash >"${log_file}" 2>&1; then
    if [ ! -e "${primary_path}" ] && [ ! -e "${legacy_path}" ]; then
        pass 1 "bash completion removed"
    else
        fail 1 "bash completion not removed"
    fi
else
    fail 1 "bash completion removal failed"
fi

exit "${status}"
