#!/bin/sh
# TAP test verifying bash completion installation from img2sixel.

set -eux

command -v bash >/dev/null || {
    printf "1..0 # SKIP bash is not found\n";
    exit 0
}

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}


printf '1..1\n'
set -v
mkdir -p "${ARTIFACT_LOCAL_DIR}"

completion_home="${ARTIFACT_LOCAL_DIR}"
target_path="${completion_home}/.local/share/bash-completion/completions/img2sixel"
legacy_path="${completion_home}/.bash_completion.d/img2sixel"

trap 'rm -rf "${target_path}" "${legacy_path}"' EXIT INT TERM

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --env IMG2SIXEL_COMPLETION_HOME="${completion_home}" \
              --env IMG2SIXEL_COMPLETION_DIR="${TOP_SRCDIR}/converters/shell-completion" \
              --env BASH_VERSION=5.0 \
              -2 bash >&2 || {
    echo "not ok" 1 - "bash completion install failed"
    exit 0
}

test -f "${target_path}" && \
    grep '# bash completion for img2sixel' "${target_path}" >/dev/null 2>&1 && {
    echo "ok" 1 - "bash completion installed"
    exit 0
}

test -f "${legacy_path}" && \
    grep '# bash completion for img2sixel' "${legacy_path}" >/dev/null 2>&1 && {
    echo "ok" 1 - "bash completion installed"
    exit 0
}

echo "not ok" 1 - "bash completion not installed"

exit 0
