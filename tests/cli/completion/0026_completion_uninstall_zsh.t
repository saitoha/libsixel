#!/bin/sh
# TAP test verifying zsh completion removal from img2sixel.

set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}

completion_home="${ARTIFACT_LOCAL_DIR}"
target_path="${completion_home}/.zfunc/_img2sixel"

printf '1..1\n'
set -v

IMG2SIXEL_COMPLETION_HOME="${completion_home}"
export IMG2SIXEL_COMPLETION_HOME

run_img2sixel -3 zsh >/dev/null || {
    fail 1 "zsh completion removal failed"
    exit 0
}

test ! -e "${target_path}" || {
    fail 1 "zsh completion not removed"
    exit 0
}

pass 1 "zsh completion removed"

exit 0
