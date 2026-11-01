#!/bin/sh
# Verify dense packing through both registered lookup bases.
# Registry row: SIXEL_OPTION_SCHEMA_LUT_POLICY|SIXEL_LOOKUP_BASE_SET_DENSE|packing
# Registry binding: lut_policy_packing|lut_policy_packing_override
# Lookup contract: packing=morton

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
short_output="${artifact_dir}/0136-dense-packing-short-$$.six"
env_output="${artifact_dir}/0136-dense-packing-env-$$.six"

short_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract,lookup_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -L builtin:cms_engine=none! --threading-policy=greedy:global_budget=1 -v --precision-policy=8bit -Wgamma \
    "-~5bit:Pmorton" "${input_image}" 2>&1 >"${short_output}") || {
    echo "not ok" 1 - "5bit packing short conversion failed"
    exit 0
}
case "${short_trace}" in *work=rgb888**) true ;; *) false ;; esac || {
    echo "not ok 1 - byte lookup did not use rgb888 working samples"
    exit 0
}
case "${short_trace}" in *LSXLUT2\|phase=palette-apply\|selected=lookup/5bit.8bit\|explicit=1**) true ;; *) false ;; esac || {
    echo "not ok 1 - explicit byte lookup missed palette application"
    exit 0
}

case "${short_trace}" in *LSXSUB1\|*key=packing\|stored=1\|binding=lut_policy_packing,lut_policy_packing_override\|value=1**) true ;; *) false ;; esac || {
    echo "not ok" 1 - "5bit packing short value was not stored"
    exit 0
}
case "${short_trace}" in *LSXLUT1\|*packing=morton**) true ;; *) false ;; esac || {
    echo "not ok" 1 - "packing missed the 5bit consumer"
    exit 0
}

env_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract,lookup_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -L builtin:cms_engine=none! --threading-policy=greedy:global_budget=1 -v --precision-policy=8bit -Wgamma \
    --env "SIXEL_LOOKUP_PACKING=MoRtOn" "-~5bit" \
    "${input_image}" 2>&1 >"${env_output}") || {
    echo "not ok" 1 - "5bit packing environment conversion failed"
    exit 0
}
case "${env_trace}" in *work=rgb888**) true ;; *) false ;; esac || {
    echo "not ok 1 - byte lookup did not use rgb888 working samples"
    exit 0
}
case "${env_trace}" in *LSXLUT2\|phase=palette-apply\|selected=lookup/5bit.8bit\|explicit=1**) true ;; *) false ;; esac || {
    echo "not ok 1 - explicit byte lookup missed palette application"
    exit 0
}

case "${env_trace}" in *LSXSUB1\|*key=packing\|stored=1\|binding=lut_policy_packing,lut_policy_packing_override\|value=1**) true ;; *) false ;; esac || {
    echo "not ok" 1 - "5bit packing environment value was not stored"
    exit 0
}
case "${env_trace}" in *LSXLUT1\|*packing=morton**) true ;; *) false ;; esac || {
    echo "not ok" 1 - "environment packing missed the 5bit consumer"
    exit 0
}

cmp -s "${short_output}" "${env_output}" || {
    echo "not ok" 1 - "5bit packing outputs differ"
    exit 0
}

bit6_short_trace=$(set +xv; \
    SIXEL_TRACE_TOPIC=suboption_contract,lookup_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -L builtin:cms_engine=none! --threading-policy=greedy:global_budget=1 -v --precision-policy=8bit -Wgamma \
    "-~6bit:Pmorton" "${input_image}" 2>&1 >/dev/null) || {
    echo "not ok" 1 - "6bit packing short conversion failed"
    exit 0
}
case "${bit6_short_trace}" in *LSXSUB1\|*key=packing\|stored=1\|binding=lut_policy_packing,lut_policy_packing_override\|value=1**) true ;; *) false ;; esac || {
    echo "not ok" 1 - "6bit packing short value was not stored"
    exit 0
}
case "${bit6_short_trace}" in *LSXLUT1\|*packing=morton**) true ;; *) false ;; esac || {
    echo "not ok" 1 - "packing missed the 6bit consumer"
    exit 0
}

bit6_env_trace=$(set +xv; \
    SIXEL_TRACE_TOPIC=suboption_contract,lookup_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -L builtin:cms_engine=none! --threading-policy=greedy:global_budget=1 -v --precision-policy=8bit -Wgamma \
    --env "SIXEL_LOOKUP_PACKING=MoRtOn" "-~6bit" \
    "${input_image}" 2>&1 >/dev/null) || {
    echo "not ok" 1 - "6bit packing environment conversion failed"
    exit 0
}
case "${bit6_env_trace}" in *LSXSUB1\|*key=packing\|stored=1\|binding=lut_policy_packing,lut_policy_packing_override\|value=1**) true ;; *) false ;; esac || {
    echo "not ok" 1 - "6bit packing environment value was not stored"
    exit 0
}
case "${bit6_env_trace}" in *LSXLUT1\|*packing=morton**) true ;; *) false ;; esac || {
    echo "not ok" 1 - "environment packing missed the 6bit consumer"
    exit 0
}

set +e
${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -L builtin:cms_engine=none! --threading-policy=greedy:global_budget=1 -v --precision-policy=8bit -Wgamma "-~fhedt:Pmorton" \
    "${input_image}" >/dev/null 2>&1
invalid_status=$?
set -e
test "${invalid_status}" -ne 0 || {
    echo "not ok" 1 - "packing escaped its declared lookup bases"
    exit 0
}

# The observation is option routing and exact output equivalence.
# Image quality belongs to the dither, palette and loader quality suites.
test -s "${short_output}" || {
    echo "not ok 1 - option output is empty"
    exit 0
}

echo "ok" 1 - "dense packing preserves both lookup consumers"
exit 0
