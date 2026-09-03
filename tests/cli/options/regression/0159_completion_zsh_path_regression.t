#!/bin/sh
# Verify completion zsh_path through short and environment paths.
# Registry row: SIXEL_OPTION_SCHEMA_COMPLETION_POLICY|NULL|zsh_path
# Registry binding: zsh_path|zsh_path_override
# Completion contract: key=zsh_path|configured=1|used=1

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    echo "1..0 # SKIP img2sixel is disabled in this build"
    exit 0
}

echo "1..1"
set -v

completion_path="${TOP_SRCDIR}/tests/data/completion/sources/zsh/_img2sixel"
short_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract,completion_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env "IMG2SIXEL_COMPLETION_ZSH=/missing-zsh-completion" \
    -K "auto:Z${completion_path}" -1 zsh 2>&1) || {
    echo "not ok" 1 - "completion zsh_path short lookup failed"
    exit 0
}
test "${short_trace#*LSXSUB1|*key=zsh_path|stored=1|binding=zsh_path,zsh_path_override|value="${completion_path}"*}" != "${short_trace}" || {
    echo "not ok" 1 - "completion zsh_path short value was not stored"
    exit 0
}
test "${short_trace#*LSXCMP1|*key=zsh_path|configured=1|override=1|used=1*}" != "${short_trace}" || {
    echo "not ok" 1 - "completion zsh_path short value missed its consumer"
    exit 0
}
test "${short_trace#*completion zsh path fixture*}" != "${short_trace}" || {
    echo "not ok" 1 - "completion zsh_path short source was not read"
    exit 0
}

env_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract,completion_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env "IMG2SIXEL_COMPLETION_ZSH=${completion_path}" -1 zsh 2>&1) || {
    echo "not ok" 1 - "completion zsh_path environment lookup failed"
    exit 0
}
test "${env_trace#*LSXCMP1|*key=zsh_path|configured=1|override=0|used=1*}" != "${env_trace}" || {
    echo "not ok" 1 - "completion zsh_path environment missed its consumer"
    exit 0
}
test "${env_trace#*completion zsh path fixture*}" != "${env_trace}" || {
    echo "not ok" 1 - "completion zsh_path environment source was not read"
    exit 0
}

echo "ok" 1 - "completion zsh_path preserves source selection"
exit 0
