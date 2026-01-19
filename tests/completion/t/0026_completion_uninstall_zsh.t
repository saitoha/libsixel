#!/bin/sh
# TAP test verifying zsh completion removal from img2sixel.

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

completion_home="${artifact_dir}/home"
target_path="${completion_home}/.zfunc/_img2sixel"

rm -rf "${completion_home}"
mkdir -p "$(dirname "${target_path}")"
printf '# stub completion\n' >"${target_path}"

printf '1..1\n'

if IMG2SIXEL_COMPLETION_HOME="${completion_home}" \
        run_img2sixel -3 zsh >"${log_file}" 2>&1; then
    if [ ! -e "${target_path}" ]; then
        pass 1 "zsh completion removed"
    else
        fail 1 "zsh completion not removed"
    fi
else
    fail 1 "zsh completion removal failed"
fi

exit "${status}"
