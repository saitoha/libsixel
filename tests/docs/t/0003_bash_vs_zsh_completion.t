#!/bin/sh
# TAP test comparing img2sixel bash and zsh completion definitions.

set -euxv

test_name=$(basename "$0")
test_dir=$(CDPATH=; cd "$(dirname "$0")" && pwd)
category_name=$(basename "$(dirname "${test_dir}")")
artifact_root=${ARTIFACT_ROOT:-"$(pwd)/_artifacts"}
artifact_dir="${artifact_root}/${category_name}/${test_name}"
log_file="${artifact_dir}/documentation.log"
bash_opts="${artifact_dir}/options-bash.txt"
zsh_opts="${artifact_dir}/options-zsh.txt"
bash_sorted="${artifact_dir}/options-bash-sorted.txt"
zsh_sorted="${artifact_dir}/options-zsh-sorted.txt"

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

die_skip() {
    reason=$1
    echo "1..1"
    echo "ok 1 - skip ${reason}"
    exit 0
}

require_file "${top_srcdir}/converters/shell-completion/bash/img2sixel"
require_file "${top_srcdir}/converters/shell-completion/zsh/_img2sixel"

if ! command -v diff >/dev/null 2>&1; then
    die_skip "diff not available"
fi

printf '1..1\n'

if grep ' --[0-9a-zA-Z_@=~%?]' \
        "${top_srcdir}/converters/shell-completion/bash/img2sixel" \
        | grep -v "' " \
        | sed 's/.* \(-.\) .*/\1/' >"${bash_opts}"; then
    :
else
    fail 1 "failed to parse bash completion"
fi

if LC_ALL=C sort "${bash_opts}" >"${bash_sorted}"; then
    :
else
    fail 1 "failed to sort bash completion"
fi

if grep '{-' "${top_srcdir}/converters/shell-completion/zsh/_img2sixel" \
        | cut -f1 -d, \
        | cut -f2 -d'{' >"${zsh_opts}"; then
    :
else
    fail 1 "failed to parse zsh completion"
fi

if LC_ALL=C sort "${zsh_opts}" >"${zsh_sorted}"; then
    :
else
    fail 1 "failed to sort zsh completion"
fi

if diff -u "${bash_sorted}" "${zsh_sorted}" >>"${log_file}" 2>&1; then
    pass 1 "bash completion matches zsh completion"
else
    fail 1 "bash completion diverges from zsh completion"
fi

exit "${status}"