#!/bin/sh
# Verify max_seconds reaches both progressive presentation consumers.
# Policy: docs/encoder/output/delta-encoding.md
# Registry row: SIXEL_OPTION_SCHEMA_UPDATE_POLICY|SIXEL_UPDATE_BASE_SET_REPAIR|max_seconds
# Registry binding: progressive_max_seconds|progressive_seconds_override
# Image assertion: option-values-and-output-equivalence
set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    echo "1..0 # SKIP img2sixel is disabled"
    exit 0
}

echo "1..1"
set -v

# One pass makes output deterministic even when the platform clock has not
# advanced. Inspect the budget held by the actual presentation consumer; the
# injected-clock C tests own deadline and admitted-packet stop behavior.
test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"
input_image="${TOP_SRCDIR}/tests/data/inputs/snake_16.png"
artifact_dir="${ARTIFACT_LOCAL_DIR}"
short_output="${artifact_dir}/0177-max_seconds-short-$$.six"
env_output="${artifact_dir}/0177-max_seconds-env-$$.six"

short_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -v -Wgamma --threading-policy=greedy:global_budget=1 \
    -Z progressive:N1:M1e-300 "${input_image}" 2>&1 >"${short_output}") || {
    echo "not ok 1 - short max_seconds conversion failed"
    exit 0
}
case "${short_trace}" in *LSXSUB1\|*key=max_seconds\|stored=1\|binding=progressive_max_seconds,progressive_seconds_override\|value=1e-300**) true ;; *) false ;; esac || {
    echo "not ok 1 - short max_seconds assignment missing"
    exit 0
}
case "${short_trace}" in *"progressive: stop=max_passes passes=1"*"max_seconds=1e-300"*) true ;; *) false ;; esac || {
    echo "not ok 1 - short max_seconds missed the presentation consumer"
    exit 0
}

env_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -v -Wgamma --threading-policy=greedy:global_budget=1 \
    --env "SIXEL_UPDATE_PROGRESSIVE_MAX_SECONDS=1e-300" -Z progressive:N1 "${input_image}" 2>&1 >"${env_output}") || {
    echo "not ok 1 - env max_seconds conversion failed"
    exit 0
}
case "${env_trace}" in *LSXSUB1\|*key=max_seconds\|stored=1\|binding=progressive_max_seconds,progressive_seconds_override\|value=1e-300**) true ;; *) false ;; esac || {
    echo "not ok 1 - env max_seconds assignment missing"
    exit 0
}
case "${env_trace}" in *"progressive: stop=max_passes passes=1"*"max_seconds=1e-300"*) true ;; *) false ;; esac || {
    echo "not ok 1 - env max_seconds missed the presentation consumer"
    exit 0
}

delta_short_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -v -Wgamma --threading-policy=greedy:global_budget=1 \
    -Z delta:P1:N1:M1e-300 "${input_image}" 2>&1 >/dev/null) || {
    echo "not ok 1 - delta_short max_seconds conversion failed"
    exit 0
}
case "${delta_short_trace}" in *LSXSUB1\|*key=max_seconds\|stored=1\|binding=progressive_max_seconds,progressive_seconds_override\|value=1e-300**) true ;; *) false ;; esac || {
    echo "not ok 1 - delta_short max_seconds assignment missing"
    exit 0
}
case "${delta_short_trace}" in *"progressive: stop=max_passes passes=1"*"max_seconds=1e-300"*) true ;; *) false ;; esac || {
    echo "not ok 1 - delta_short max_seconds missed the presentation consumer"
    exit 0
}

delta_env_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -v -Wgamma --threading-policy=greedy:global_budget=1 \
    --env "SIXEL_UPDATE_PROGRESSIVE_MAX_SECONDS=1e-300" -Z delta:P1:N1 "${input_image}" 2>&1 >/dev/null) || {
    echo "not ok 1 - delta_env max_seconds conversion failed"
    exit 0
}
case "${delta_env_trace}" in *LSXSUB1\|*key=max_seconds\|stored=1\|binding=progressive_max_seconds,progressive_seconds_override\|value=1e-300**) true ;; *) false ;; esac || {
    echo "not ok 1 - delta_env max_seconds assignment missing"
    exit 0
}
case "${delta_env_trace}" in *"progressive: stop=max_passes passes=1"*"max_seconds=1e-300"*) true ;; *) false ;; esac || {
    echo "not ok 1 - delta_env max_seconds missed the presentation consumer"
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
