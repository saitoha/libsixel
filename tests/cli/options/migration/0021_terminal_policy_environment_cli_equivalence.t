#!/bin/sh
# Verify terminal cursor policy is equivalent through CLI and env paths.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    echo "1..0 # SKIP img2sixel is disabled in this build"
    exit 0
}

echo "1..1"
set -v

test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"
input_image="${TOP_SRCDIR}/tests/data/inputs/snake_motion_64_2frame.gif"
cli_output="${ARTIFACT_LOCAL_DIR}/terminal-policy-cli-$$.six"
env_output="${ARTIFACT_LOCAL_DIR}/terminal-policy-env-$$.six"

cli_trace=$(set +xv; SIXEL_TRACE_TOPIC=terminal_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -l disable -z1 \
    "${input_image}" -o "${cli_output}" 2>&1) || {
    echo "not ok" 1 - "terminal policy CLI conversion failed"
    exit 0
}
test "${cli_trace#*LSXTTY1\|hide_cursor=1\|override=1\|*}" != \
    "${cli_trace}" || {
    echo "not ok" 1 - "terminal policy CLI value missed its consumer"
    exit 0
}

env_trace=$(set +xv; SIXEL_TRACE_TOPIC=terminal_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -l disable \
    --env SIXEL_ANIMATION_HIDE_CURSOR=1 \
    "${input_image}" -o "${env_output}" 2>&1) || {
    echo "not ok" 1 - "terminal policy environment conversion failed"
    exit 0
}
test "${env_trace#*LSXTTY1\|hide_cursor=1\|override=0\|*}" != \
    "${env_trace}" || {
    echo "not ok" 1 - "terminal policy environment missed its consumer"
    exit 0
}

empty_trace=$(set +xv; SIXEL_TRACE_TOPIC=terminal_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -l disable \
    --env SIXEL_ANIMATION_HIDE_CURSOR= \
    "${input_image}" -o /dev/null 2>&1) || {
    echo "not ok" 1 - "empty terminal policy conversion failed"
    exit 0
}
test "${empty_trace#*LSXTTY1\|hide_cursor=0\|override=0\|*}" != \
    "${empty_trace}" || {
    echo "not ok" 1 - "empty terminal policy no longer disables hiding"
    exit 0
}

override_trace=$(set +xv; SIXEL_TRACE_TOPIC=terminal_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -l disable \
    --env SIXEL_ANIMATION_HIDE_CURSOR=1 -z0 \
    "${input_image}" -o /dev/null 2>&1) || {
    echo "not ok" 1 - "terminal policy override conversion failed"
    exit 0
}
test "${override_trace#*LSXTTY1\|hide_cursor=0\|override=1\|*}" != \
    "${override_trace}" || {
    echo "not ok" 1 - "terminal policy CLI did not override its environment"
    exit 0
}

cmp -s "${cli_output}" "${env_output}" || {
    echo "not ok" 1 - "terminal policy CLI and environment outputs differ"
    exit 0
}
echo "ok" 1 - "terminal policy CLI matches its environment"
exit 0
