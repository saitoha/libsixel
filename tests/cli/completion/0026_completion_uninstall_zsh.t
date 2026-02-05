#!/bin/sh
# TAP test verifying zsh completion removal from img2sixel.

set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

status=0

ensure_converter_available "IMG2SIXEL" "${IMG2SIXEL_PATH}" "img2sixel"

completion_home="${ARTIFACT_LOCAL_DIR}"
target_path="${completion_home}/.zfunc/_img2sixel"

printf '1..1\n'
set -v

IMG2SIXEL_COMPLETION_HOME="${completion_home}"
export IMG2SIXEL_COMPLETION_HOME

if run_img2sixel -3 zsh > "${ARTIFACT_LOCAL_DIR}/output.txt"; then
    if [ ! -e "${target_path}" ]; then
        pass 1 "zsh completion removed"
    else
        fail 1 "zsh completion not removed"
    fi
else
    fail 1 "zsh completion removal failed"
fi

exit "${status}"
