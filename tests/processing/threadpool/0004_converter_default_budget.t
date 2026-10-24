#!/bin/sh
# Policy: docs/cli/threading-policy.md
# Verify application budget defaults and user overrides at their consumers.
set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    echo "1..0 # SKIP img2sixel is disabled"
    exit 0
}
test "${HAVE_SIXEL2PNG-}" = 1 || {
    echo "1..0 # SKIP sixel2png is disabled"
    exit 0
}
echo "1..1"
set -v

counts=$(${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    threadpool/0002_thread_default_counts converter-counts) || {
    echo "not ok 1 - topology probe failed"
    exit 0
}
read -r expected hardware limited <<EOF_COUNTS
${counts}
EOF_COUNTS

# Empty application variables must receive the converter defaults.
trace=$(set +xv; SIXEL_THREADING_POLICY='' SIXEL_THREADING_GLOBAL_BUDGET='' \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -v \
    "${TOP_SRCDIR}/tests/data/inputs/snake_64.png" -o /dev/null 2>&1) || {
    echo "not ok 1 - default conversion failed"
    exit 0
}
case "${trace}" in *"threading: mode=balanced budget=${expected} "*) true ;; *) false ;; esac || {
    echo "not ok 1 - converter default did not resolve all performance CPUs"
    exit 0
}
trace=$(set +xv; SIXEL_THREADING_GLOBAL_BUDGET='' \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -v -=lazy \
    "${TOP_SRCDIR}/tests/data/inputs/snake_64.png" -o /dev/null 2>&1) || {
    echo "not ok 1 - lazy conversion failed"
    exit 0
}
case "${trace}" in *"threading: mode=lazy budget=${hardware} "*) true ;; *) false ;; esac || {
    echo "not ok 1 - lazy converter lost its all budget"
    exit 0
}
trace=$(set +xv; SIXEL_THREADING_POLICY='' SIXEL_THREADING_GLOBAL_BUDGET=2 \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -v \
    "${TOP_SRCDIR}/tests/data/inputs/snake_64.png" -o /dev/null 2>&1) || {
    echo "not ok 1 - environment conversion failed"
    exit 0
}
case "${trace}" in *"threading: mode=balanced budget=${limited} "*) true ;; *) false ;; esac || {
    echo "not ok 1 - converter replaced the user environment"
    exit 0
}
trace=$(set +xv; SIXEL_THREADING_GLOBAL_BUDGET=all \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -v \
    --env SIXEL_THREADING_GLOBAL_BUDGET=1 -=greedy \
    "${TOP_SRCDIR}/tests/data/inputs/snake_64.png" -o /dev/null 2>&1) || {
    echo "not ok 1 - --env conversion failed"
    exit 0
}
case "${trace}" in *"threading: mode=greedy budget=1 "*) true ;; *) false ;; esac || {
    echo "not ok 1 - converter replaced --env"
    exit 0
}

# Inspect actual parser starts: the default decoder has no planner summary.
# The fixture has enough payload for every ordinary host's logical CPUs.
test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"
timeline="${ARTIFACT_LOCAL_DIR}/0004-default-decode-$$.jsonl"
SIXEL_THREADING_POLICY='' SIXEL_THREADING_GLOBAL_BUDGET='' \
    ${SIXEL_RUNTIME-} "${SIXEL2PNG_PATH}" -J "${timeline}" \
    -i "${TOP_SRCDIR}/tests/data/inputs/snake_64.six" -o /dev/null || {
    echo "not ok 1 - default decoder failed"
    exit 0
}
parse_starts=0
while IFS= read -r line; do
    case "${line}" in *\"worker\":\"decoder\",\"role\":\"parse\",\"event\":\"start\"*) false ;; *) true ;; esac || parse_starts=$((parse_starts + 1))
done < "${timeline}"
test "${expected}" -gt 1 || expected=0
test "${parse_starts}" -eq "${expected}" || {
    echo "not ok 1 - decoder default did not use all performance CPUs"
    exit 0
}
echo "ok 1 - converter all defaults and user overrides reach consumers"
exit 0
