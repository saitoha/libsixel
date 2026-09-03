#!/bin/sh
# Verify completion directory through short and environment paths.
# Registry row: SIXEL_OPTION_SCHEMA_COMPLETION_POLICY|NULL|directory
# Registry binding: directory|directory_override
# Completion contract: key=directory|configured=1|used=1

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    echo "1..0 # SKIP img2sixel is disabled in this build"
    exit 0
}

echo "1..1"
set -v

completion_dir="${TOP_SRCDIR}/tests/data/completion/sources"
short_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract,completion_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env "IMG2SIXEL_COMPLETION_DIR=/missing-completion-directory" \
    -K "auto:D${completion_dir}" -1 bash 2>&1) || {
    echo "not ok" 1 - "completion directory short lookup failed"
    exit 0
}
test "${short_trace#*LSXSUB1|*key=directory|stored=1|binding=directory,directory_override|value="${completion_dir}"*}" != "${short_trace}" || {
    echo "not ok" 1 - "completion directory short value was not stored"
    exit 0
}
test "${short_trace#*LSXCMP1|*key=directory|configured=1|override=1|used=1*}" != "${short_trace}" || {
    echo "not ok" 1 - "completion directory short value missed its consumer"
    exit 0
}
test "${short_trace#*completion bash path fixture*}" != "${short_trace}" || {
    echo "not ok" 1 - "completion directory short source was not read"
    exit 0
}

env_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract,completion_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env "IMG2SIXEL_COMPLETION_DIR=${completion_dir}" -1 bash 2>&1) || {
    echo "not ok" 1 - "completion directory environment lookup failed"
    exit 0
}
test "${env_trace#*LSXCMP1|*key=directory|configured=1|override=0|used=1*}" != "${env_trace}" || {
    echo "not ok" 1 - "completion directory environment missed its consumer"
    exit 0
}
test "${env_trace#*completion bash path fixture*}" != "${env_trace}" || {
    echo "not ok" 1 - "completion directory environment source was not read"
    exit 0
}

echo "ok" 1 - "completion directory preserves source selection"
exit 0
