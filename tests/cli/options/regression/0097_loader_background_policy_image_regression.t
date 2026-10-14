#!/bin/sh
# Verify common loader background policy through short and environment paths.
# Registry row: SIXEL_OPTION_SCHEMA_LOADERS|NULL|background_policy
# Registry binding: background_policy
# Policy: docs/loader/background-policy.md

# Image assertion: option-values-and-output-equivalence
set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v

# Preserve the unmanaged fixture values; CMS defaults are tested separately.

test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"

input_image="${TOP_SRCDIR}/images/pngsuite/background/bgbn4a08.png"
artifact_dir="${ARTIFACT_LOCAL_DIR}"
short_output="${artifact_dir}/0097-loader-background-policy-short-$$.six"
env_output="${artifact_dir}/0097-loader-background-policy-env-$$.six"
control_output="${artifact_dir}/0097-loader-background-policy-control-$$.six"

short_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --cms-engine=none \
    --env "SIXEL_BACKGROUND_POLICY=file_first" \
    "-Lbuiltin:Pexplicit_first!" -B#fff "${input_image}" \
    2>&1 >"${short_output}") || {
    echo "not ok" 1 - "background_policy short conversion failed"
    exit 0
}

case "${short_trace}" in *LSXSUB1\|*key=background_policy\|stored=1\|binding=background_policy\|value=1**) true ;; *) false ;; esac || {
    echo "not ok" 1 - "background_policy short value was not stored"
    exit 0
}

env_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --cms-engine=none \
    --env "SIXEL_BACKGROUND_POLICY=explicit_first" "-Lbuiltin!" \
    -B#fff "${input_image}" 2>&1 >"${env_output}") || {
    echo "not ok" 1 - "background_policy environment conversion failed"
    exit 0
}

case "${env_trace}" in *LSXSUB1\|*key=background_policy\|stored=1\|binding=background_policy\|value=1**) true ;; *) false ;; esac || {
    echo "not ok" 1 - "background_policy environment value was not stored"
    exit 0
}

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --cms-engine=none \
    --env "SIXEL_BACKGROUND_POLICY=file_first" "-Lbuiltin!" \
    -B#fff "${input_image}" >"${control_output}" || {
    echo "not ok" 1 - "background_policy control conversion failed"
    exit 0
}

cmp -s "${short_output}" "${env_output}" || {
    echo "not ok" 1 - "background_policy short and env output differ"
    exit 0
}

cmp -s "${short_output}" "${control_output}" && {
    echo "not ok" 1 - "background_policy did not affect image output"
    exit 0
}

# The observation is option routing and exact output equivalence.
# Image quality belongs to the dither, palette and loader quality suites.
test -s "${short_output}" || {
    echo "not ok 1 - option output is empty"
    exit 0
}

echo "ok" 1 - "background_policy preserves image output"
exit 0
