#!/bin/sh
# TAP test verifying zsh install does not duplicate shell rc lines.

set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

command -v zsh >/dev/null || skip_all "zsh is not found"

completion_home="${ARTIFACT_LOCAL_DIR}"
rc_path="${completion_home}/.zshrc"
fpath_count=0
compinit_count=0

ensure_converter_available "IMG2SIXEL" "${IMG2SIXEL_PATH}" "img2sixel"

echo '1..1'
set -v

run_img2sixel --env IMG2SIXEL_COMPLETION_HOME="${completion_home}" \
    -- -2 zsh >/dev/null || {
    fail 1 "zsh completion install failed on first run"
    exit 0
}

run_img2sixel --env IMG2SIXEL_COMPLETION_HOME="${completion_home}" \
    -- -2 zsh >/dev/null || {
    fail 1 "zsh completion install failed on second run"
    exit 0
}

fpath_count=$(grep -c "^fpath+=(\"\$HOME/.zfunc\")$" "${rc_path}")
compinit_count=$(grep -c '^autoload -Uz compinit && compinit -u$' "${rc_path}")

test "${fpath_count}" -eq 1 || {
    fail 1 "zsh fpath line was duplicated"
    exit 0
}

test "${compinit_count}" -eq 1 || {
    fail 1 "zsh compinit line was duplicated"
    exit 0
}

pass 1 "zsh rc lines stay unique across repeated installs"
exit 0
