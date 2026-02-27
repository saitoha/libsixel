#!/bin/sh
# TAP test verifying bash completion removal from img2sixel.

set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}

printf '1..1\n'
set -v

completion_home="${ARTIFACT_LOCAL_DIR}"
primary_path="${completion_home}/.local/share/bash-completion/completions/img2sixel"
legacy_path="${completion_home}/.bash_completion.d/img2sixel"

run_img2sixel --env IMG2SIXEL_COMPLETION_HOME="${completion_home}" \
              -3 bash >/dev/null || {
    fail 1 "bash completion removal failed"
    exit 0
}

test ! -e "${primary_path}" || {
    fail 1 "bash completion not removed"
    exit 0
}

test ! -e "${legacy_path}" || {
    fail 1 "bash completion not removed"
    exit 0
}

pass 1 "bash completion removed"
exit 0
