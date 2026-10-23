#!/bin/sh
# Policy: docs/cli/correction-suggestions.md
# Verify fuzzy suggestions through short and environment paths.
# Registry row: SIXEL_OPTION_SCHEMA_DIAGNOSTICS|NULL|fuzzy_suggestions
# Registry binding: fuzzy_suggestions|fuzzy_suggestions_override

# Image assertion: option-values-and-output-equivalence
set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    echo "1..0 # SKIP img2sixel is disabled in this build"
    exit 0
}

echo "1..1"
set -v

test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"
input_image="${TOP_SRCDIR}/tests/data/inputs/snake_16.png"
artifact_dir="${ARTIFACT_LOCAL_DIR}"
short_output="${artifact_dir}/0148-diagnostics-fuzzy-short-$$.six"
env_output="${artifact_dir}/0148-diagnostics-fuzzy-env-$$.six"

short_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env "SIXEL_OPTION_FUZZY_SUGGESTIONS=1" "-xhuman:F0" \
    "${input_image}" 2>&1 >"${short_output}") || {
    echo "not ok" 1 - "fuzzy suggestions short conversion failed"
    exit 0
}
case "${short_trace}" in *LSXSUB1\|*key=fuzzy_suggestions\|stored=1\|binding=fuzzy_suggestions,fuzzy_suggestions_override\|value=0**) true ;; *) false ;; esac || {
    echo "not ok" 1 - "fuzzy suggestions short value was not stored"
    exit 0
}

env_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env "SIXEL_OPTION_FUZZY_SUGGESTIONS=0" -xhuman \
    "${input_image}" 2>&1 >"${env_output}") || {
    echo "not ok" 1 - "fuzzy suggestions environment conversion failed"
    exit 0
}
case "${env_trace}" in *LSXSUB1\|*key=fuzzy_suggestions\|stored=1\|binding=fuzzy_suggestions,fuzzy_suggestions_override\|value=0**) true ;; *) false ;; esac || {
    echo "not ok" 1 - "fuzzy suggestions environment value was not stored"
    exit 0
}

status=0
message=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env "SIXEL_OPTION_FUZZY_SUGGESTIONS=1" \
    -xhuman:F0 -r hamnimg "${input_image}" -o/dev/null 2>&1) || status=$?
test "${status}" -eq 2 || {
    echo "not ok" 1 - "fuzzy suggestions consumer exit status mismatch"
    exit 0
}
case "${message}" in *Did\ you\ mean:**) false ;; *) true ;; esac || {
    echo "not ok" 1 - "fuzzy suggestions remained enabled"
    exit 0
}

cmp -s "${short_output}" "${env_output}" || {
    echo "not ok" 1 - "fuzzy suggestions outputs differ"
    exit 0
}
# The observation is option routing and exact output equivalence.
# Image quality belongs to the dither, palette and loader quality suites.
test -s "${short_output}" || {
    echo "not ok 1 - option output is empty"
    exit 0
}

echo "ok" 1 - "fuzzy suggestions preserve image and diagnostic output"
exit 0
