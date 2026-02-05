#!/bin/sh
# TAP test verifying bash completion installation from img2sixel.

set -eux



script_dir=$(CDPATH=; cd "$(dirname "$0")" && pwd)
. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

if ! command -v bash >/dev/null; then
    skip_all "bash is not found"
fi

status=0

ensure_converter_available "IMG2SIXEL" "${IMG2SIXEL_PATH}" "img2sixel"



completion_dir="${top_srcdir}/converters/shell-completion"
completion_dir=$(printf '%s' "${completion_dir}" | tr '\\\\' '/')
completion_source="${completion_dir}/bash/img2sixel"

require_file "${completion_source}"

# Use a writable temporary home to avoid permission issues on shared
# workspaces while still keeping logs under tests/_artifacts.
completion_home=""
if command -v mktemp >/dev/null 2>&1; then
    completion_home=$(mktemp -d "${TMPDIR:-/tmp}/img2sixel-home.XXXXXX")
else
    completion_home="${ARTIFACT_LOCAL_DIR}/home.$$"
fi
if [ -z "${completion_home}" ]; then
    echo "Failed to create a temporary home directory" >&2
    exit 1
fi

cleanup_home() {
    rm -rf "${completion_home}"
}
trap cleanup_home EXIT INT TERM

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

if run_img2sixel -2 bash; then
    if [ -f "${target_path}" ] && \
            grep -F '# bash completion for img2sixel' \
            "${target_path}" >/dev/null 2>&1; then
        pass 1 "bash completion installed"
    elif [ -f "${legacy_path}" ] && \
            grep -F '# bash completion for img2sixel' \
            "${legacy_path}" >/dev/null 2>&1; then
        pass 1 "bash completion installed"
    else
        fail 1 "bash completion not installed"
    fi
else
    fail 1 "bash completion install failed"
fi

exit "${status}"
