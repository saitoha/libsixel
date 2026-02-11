#!/bin/sh
# TAP test verifying zsh install does not duplicate shell rc lines.

set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

status=0
completion_home=""
rc_path=""
fpath_count=0
compinit_count=0

ensure_converter_available "IMG2SIXEL" "${IMG2SIXEL_PATH}" "img2sixel"

if command -v mktemp >/dev/null 2>&1; then
    completion_home=$(mktemp -d "${TMPDIR:-/tmp}/img2sixel-home.XXXXXX")
else
    completion_home="${ARTIFACT_LOCAL_DIR}/home-zsh.$$"
fi

if [ -z "${completion_home}" ]; then
    echo "Failed to create a temporary home directory" >&2
    exit 1
fi

cleanup_home() {
    rm -rf "${completion_home}"
}
trap cleanup_home EXIT INT TERM

rc_path="${completion_home}/.zshrc"

echo '1..1'
set -v

if run_img2sixel --env IMG2SIXEL_COMPLETION_HOME="${completion_home}" \
        -- -2 zsh >/dev/null && \
        run_img2sixel --env IMG2SIXEL_COMPLETION_HOME="${completion_home}" \
        -- -2 zsh >/dev/null; then
    fpath_count=$(grep -c '^fpath+=("\$HOME/.zfunc")$' "${rc_path}")
    compinit_count=$(grep -c '^autoload -Uz compinit && compinit -u$' "${rc_path}")
    if [ "${fpath_count}" -eq 1 ] && [ "${compinit_count}" -eq 1 ]; then
        pass 1 "zsh rc lines stay unique across repeated installs"
    else
        fail 1 "zsh rc lines were duplicated"
    fi
else
    fail 1 "zsh completion repeated install failed"
fi

exit "${status}"
