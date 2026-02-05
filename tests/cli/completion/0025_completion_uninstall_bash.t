#!/bin/sh
# TAP test verifying bash completion removal from img2sixel.

set -eux



script_dir=$(CDPATH=; cd "$(dirname "$0")" && pwd)
. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

status=0

ensure_converter_available "IMG2SIXEL" "${IMG2SIXEL_PATH}" "img2sixel"



completion_home="${ARTIFACT_LOCAL_DIR}"
primary_path="${completion_home}/.local/share/bash-completion/completions/img2sixel"
legacy_path="${completion_home}/.bash_completion.d/img2sixel"

printf '1..1\n'
set -v

IMG2SIXEL_COMPLETION_HOME="${completion_home}"
export IMG2SIXEL_COMPLETION_HOME

if ! run_img2sixel -1 bash; then
    fail 1 "bash completion install failed"
    exit "${status}"
fi

if run_img2sixel -3 bash; then
    if [ ! -e "${primary_path}" ] && [ ! -e "${legacy_path}" ]; then
        pass 1 "bash completion removed"
    else
        fail 1 "bash completion not removed"
    fi
else
    fail 1 "bash completion removal failed"
fi

exit "${status}"
