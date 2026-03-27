#!/bin/sh
# TAP test verifying bash completion removal from img2sixel.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}


printf '1..1\n'
set -v
mkdir -p "${ARTIFACT_LOCAL_DIR}"

completion_home="${ARTIFACT_LOCAL_DIR}"
primary_path="${completion_home}/.local/share/bash-completion/completions/img2sixel"
legacy_path="${completion_home}/.bash_completion.d/img2sixel"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --env IMG2SIXEL_COMPLETION_HOME="${completion_home}" \
              -3 bash >/dev/null || {
    echo "not ok" 1 - "bash completion removal failed"
    exit 0
}

test ! -e "${primary_path}" || {
    echo "not ok" 1 - "bash completion not removed"
    exit 0
}

test ! -e "${legacy_path}" || {
    echo "not ok" 1 - "bash completion not removed"
    exit 0
}

echo "ok" 1 - "bash completion removed"
exit 0
