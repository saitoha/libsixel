#!/bin/sh
# TAP test verifying bash completion installation from img2sixel.

set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

command -v bash >/dev/null || skip_all "bash is not found"

ensure_converter_available "IMG2SIXEL" "${IMG2SIXEL_PATH}" "img2sixel"

completion_dir="${TOP_SRCDIR}/converters/shell-completion"

# Use a writable temporary home to avoid permission issues on shared
# workspaces while still keeping logs under tests/_artifacts.
completion_home=""
command -v mktemp >/dev/null 2>&1 && \
    completion_home=$(mktemp -d "${TMPDIR:-/tmp}/img2sixel-home.XXXXXX")

test -n "${completion_home}" || {
    fail 1 "failed to create a temporary home directory"
    exit 0
}

trap 'rm -rf "${completion_home}"' EXIT INT TERM

target_path="${completion_home}/.local/share/bash-completion/completions/img2sixel"
legacy_path="${completion_home}/.bash_completion.d/img2sixel"

printf '1..1\n'
set -v

IMG2SIXEL_COMPLETION_HOME="${completion_home}"
IMG2SIXEL_COMPLETION_DIR="${completion_dir}"
BASH_VERSION=5.0
export IMG2SIXEL_COMPLETION_HOME
export IMG2SIXEL_COMPLETION_DIR
export BASH_VERSION

run_img2sixel -2 bash >/dev/null || {
    fail 1 "bash completion install failed"
    exit 0
}

test -f "${target_path}" && \
    grep '# bash completion for img2sixel' "${target_path}" >/dev/null 2>&1 && {
    pass 1 "bash completion installed"
    exit 0
}

test -f "${legacy_path}" && \
    grep '# bash completion for img2sixel' "${legacy_path}" >/dev/null 2>&1 && {
    pass 1 "bash completion installed"
    exit 0
}

fail 1 "bash completion not installed"

exit 0
