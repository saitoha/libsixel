#!/bin/sh
# Verify completion home through short and environment paths.
# Registry row: IMG2SIXEL_OPTION_SCHEMA_COMPLETION_POLICY|NULL|home
# Registry binding: home|home_override
# Completion contract: key=home|configured=1|used=1

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    echo "1..0 # SKIP img2sixel is disabled in this build"
    exit 0
}

echo "1..1"
set -v

completion_home="${ARTIFACT_LOCAL_DIR}/0161-completion-home-$$"
short_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract,completion_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env "IMG2SIXEL_COMPLETION_HOME=/missing-completion-home" \
    -K "auto:H${completion_home}" -3 zsh 2>&1) || {
    echo "not ok" 1 - "completion home short lookup failed"
    exit 0
}
test "${short_trace#*LSXSUB1|*key=home|stored=1|binding=home,home_override|value="${completion_home}"*}" != "${short_trace}" || {
    echo "not ok" 1 - "completion home short value was not stored"
    exit 0
}
test "${short_trace#*LSXCMP1|*key=home|configured=1|override=1|used=1*}" != "${short_trace}" || {
    echo "not ok" 1 - "completion home short value missed its consumer"
    exit 0
}
test "${short_trace#*missing *"${completion_home##*/}"/.zfunc/_img2sixel*}" != "${short_trace}" || {
    echo "not ok" 1 - "completion home short path was not used"
    exit 0
}

env_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract,completion_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env "IMG2SIXEL_COMPLETION_HOME=${completion_home}" -3 zsh 2>&1) || {
    echo "not ok" 1 - "completion home environment lookup failed"
    exit 0
}
test "${env_trace#*LSXCMP1|*key=home|configured=1|override=0|used=1*}" != "${env_trace}" || {
    echo "not ok" 1 - "completion home environment missed its consumer"
    exit 0
}
test "${env_trace#*missing *"${completion_home##*/}"/.zfunc/_img2sixel*}" != "${env_trace}" || {
    echo "not ok" 1 - "completion home environment path was not used"
    exit 0
}

echo "ok" 1 - "completion home preserves path selection"
exit 0
