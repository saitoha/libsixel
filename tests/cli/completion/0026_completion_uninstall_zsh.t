#!/bin/sh
# TAP test verifying zsh completion removal from img2sixel.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

printf '1..1\n'
set -v
mkdir -p "${ARTIFACT_LOCAL_DIR}"

completion_home="${ARTIFACT_LOCAL_DIR}"
target_path="${completion_home}/.zfunc/_img2sixel"

run_img2sixel --env IMG2SIXEL_COMPLETION_HOME="${completion_home}" \
              -3 zsh >/dev/null || {
    echo "not ok" 1 - "zsh completion removal failed"
    exit 0
}

test ! -e "${target_path}" || {
    echo "not ok" 1 - "zsh completion not removed"
    exit 0
}

echo "ok" 1 - "zsh completion removed"
exit 0
