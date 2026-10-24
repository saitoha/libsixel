#!/bin/sh
# Verify stop_threshold reaches both progressive presentation consumers.
# Registry row: SIXEL_OPTION_SCHEMA_UPDATE_POLICY|SIXEL_UPDATE_BASE_SET_REPAIR|stop_threshold
# Registry binding: progressive_stop_threshold|progressive_stop_override
# Image assertion: option-values-and-output-equivalence
set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    echo "1..0 # SKIP img2sixel is disabled"
    exit 0
}

echo "1..1"
set -v

# These captures are independently decoded and compared.
test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"
input_image="${TOP_SRCDIR}/tests/data/inputs/snake_16.png"
artifact_dir="${ARTIFACT_LOCAL_DIR}"
short_output="${artifact_dir}/0172-stop_threshold-short-$$.six"
env_output="${artifact_dir}/0172-stop_threshold-env-$$.six"

short_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -v -Wgamma --threading-policy=greedy:global_budget=1 \
    -Z progressive:S2:M0 "${input_image}" 2>&1 >"${short_output}") || {
    echo "not ok 1 - short stop_threshold conversion failed"
    exit 0
}
case "${short_trace}" in *LSXSUB1\|*key=stop_threshold\|stored=1\|binding=progressive_stop_threshold,progressive_stop_override\|value=2**) true ;; *) false ;; esac || {
    echo "not ok 1 - short stop_threshold assignment missing"
    exit 0
}
case "${short_trace}" in *"progressive: stop=converged passes=1"*) true ;; *) false ;; esac || {
    echo "not ok 1 - short stop_threshold missed the presentation consumer"
    exit 0
}

env_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -v -Wgamma --threading-policy=greedy:global_budget=1 \
    --env "SIXEL_UPDATE_PROGRESSIVE_STOP_THRESHOLD=2" -Z progressive:M0 "${input_image}" 2>&1 >"${env_output}") || {
    echo "not ok 1 - env stop_threshold conversion failed"
    exit 0
}
case "${env_trace}" in *LSXSUB1\|*key=stop_threshold\|stored=1\|binding=progressive_stop_threshold,progressive_stop_override\|value=2**) true ;; *) false ;; esac || {
    echo "not ok 1 - env stop_threshold assignment missing"
    exit 0
}
case "${env_trace}" in *"progressive: stop=converged passes=1"*) true ;; *) false ;; esac || {
    echo "not ok 1 - env stop_threshold missed the presentation consumer"
    exit 0
}

delta_short_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -v -Wgamma --threading-policy=greedy:global_budget=1 \
    -Z delta:P1:S2:M0 "${input_image}" 2>&1 >/dev/null) || {
    echo "not ok 1 - delta_short stop_threshold conversion failed"
    exit 0
}
case "${delta_short_trace}" in *LSXSUB1\|*key=stop_threshold\|stored=1\|binding=progressive_stop_threshold,progressive_stop_override\|value=2**) true ;; *) false ;; esac || {
    echo "not ok 1 - delta_short stop_threshold assignment missing"
    exit 0
}
case "${delta_short_trace}" in *"progressive: stop=converged passes=1"*) true ;; *) false ;; esac || {
    echo "not ok 1 - delta_short stop_threshold missed the presentation consumer"
    exit 0
}

delta_env_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -v -Wgamma --threading-policy=greedy:global_budget=1 \
    --env "SIXEL_UPDATE_PROGRESSIVE_STOP_THRESHOLD=2" -Z delta:P1:M0 "${input_image}" 2>&1 >/dev/null) || {
    echo "not ok 1 - delta_env stop_threshold conversion failed"
    exit 0
}
case "${delta_env_trace}" in *LSXSUB1\|*key=stop_threshold\|stored=1\|binding=progressive_stop_threshold,progressive_stop_override\|value=2**) true ;; *) false ;; esac || {
    echo "not ok 1 - delta_env stop_threshold assignment missing"
    exit 0
}
case "${delta_env_trace}" in *"progressive: stop=converged passes=1"*) true ;; *) false ;; esac || {
    echo "not ok 1 - delta_env stop_threshold missed the presentation consumer"
    exit 0
}

cmp -s "${short_output}" "${env_output}" || {
    echo "not ok 1 - short and environment captures differ"
    exit 0
}
# This test owns exact option-route equivalence and consumer selection.
test -s "${short_output}" || {
    echo "not ok 1 - option-route output is empty"
    exit 0
}
echo "ok 1 - presentation option routes and stop reasons agree"
exit 0
