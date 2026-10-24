#!/bin/sh
# Policy: docs/cli/threading-policy.md
# Verify the encoder global budget through CLI and environment routes.
# Registry row: SIXEL_OPTION_SCHEMA_ENCODER_THREADING_POLICY|NULL|global_budget
# Registry binding: budget|budget_override
# Environment range: parse-signed-long
# Image assertion: option-values-and-output-equivalence
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

test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"

artifact_dir="${ARTIFACT_LOCAL_DIR}"

short_output="${artifact_dir}/0185-budget-six-short-$$.six"
env_output="${artifact_dir}/0185-budget-six-env-$$.six"
short_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -d fs "-=greedy:B2" \
    "${TOP_SRCDIR}/tests/data/inputs/snake_64.png" -o "${short_output}" 2>&1) || {
    echo "not ok 1 - global budget IMG2SIXEL short conversion failed"
    exit 0
}
case "${short_trace}" in *LSXSUB1\|*key=global_budget\|stored=1\|binding=budget,budget_override\|value=2**) true ;; *) false ;; esac || {
    echo "not ok 1 - global budget short route was not stored"
    exit 0
}
env_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -d fs \
    --env "SIXEL_ENCODER_THREADING_GLOBAL_BUDGET=2" "-=balanced" \
    "${TOP_SRCDIR}/tests/data/inputs/snake_64.png" -o "${env_output}" 2>&1) || {
    echo "not ok 1 - global budget IMG2SIXEL environment conversion failed"
    exit 0
}
case "${env_trace}" in *LSXSUB1\|*key=global_budget\|stored=1\|binding=budget,budget_override\|value=2**) true ;; *) false ;; esac || {
    echo "not ok 1 - global budget environment route was not stored"
    exit 0
}
range_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -d fs \
    --env "SIXEL_ENCODER_THREADING_GLOBAL_BUDGET=-1" "-=balanced" \
    "${TOP_SRCDIR}/tests/data/inputs/snake_64.png" 2>&1 >/dev/null) || {
    echo "not ok 1 - invalid budget environment should fall back"
    exit 0
}
case "${range_trace}" in *LSXSUB1\|*key=global_budget\|stored=1**) false ;; *) true ;; esac || {
    echo "not ok 1 - out-of-range budget environment was accepted"
    exit 0
}
cmp -s "${short_output}" "${env_output}" || {
    echo "not ok 1 - global budget routes changed six output"
    exit 0
}
test -s "${short_output}" || {
    echo "not ok 1 - global budget output is empty"
    exit 0
}
all_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -d fs "${TOP_SRCDIR}/tests/data/inputs/snake_64.png" "-=greedy:BaLl" \
    -o "${short_output}" 2>&1) || {
    echo "not ok 1 - IMG2SIXEL short all conversion failed"
    exit 0
}
case "${all_trace}" in *LSXSUB1\|*key=global_budget\|stored=1\|binding=budget,budget_override\|value=0**) true ;; *) false ;; esac || {
    echo "not ok 1 - IMG2SIXEL short all sentinel missing"
    exit 0
}
all_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -d fs "${TOP_SRCDIR}/tests/data/inputs/snake_64.png" --threading-policy=greedy:global_budget=ALL \
    -o "${env_output}" 2>&1) || {
    echo "not ok 1 - IMG2SIXEL long all conversion failed"
    exit 0
}
case "${all_trace}" in *LSXSUB1\|*key=global_budget\|stored=1\|binding=budget,budget_override\|value=0**) true ;; *) false ;; esac || {
    echo "not ok 1 - IMG2SIXEL long all sentinel missing"
    exit 0
}
cmp -s "${short_output}" "${env_output}" || {
    echo "not ok 1 - IMG2SIXEL long all output differs"
    exit 0
}
all_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -d fs "${TOP_SRCDIR}/tests/data/inputs/snake_64.png" --env "SIXEL_ENCODER_THREADING_GLOBAL_BUDGET=all" "-=greedy" \
    -o "${env_output}" 2>&1) || {
    echo "not ok 1 - IMG2SIXEL environment all conversion failed"
    exit 0
}
case "${all_trace}" in *LSXSUB1\|*key=global_budget\|stored=1\|binding=budget,budget_override\|value=0**) true ;; *) false ;; esac || {
    echo "not ok 1 - IMG2SIXEL environment all sentinel missing"
    exit 0
}
cmp -s "${short_output}" "${env_output}" || {
    echo "not ok 1 - IMG2SIXEL environment all output differs"
    exit 0
}

echo "ok 1 - encoder global budget routes agree"
exit 0
