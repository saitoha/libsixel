#!/bin/sh
# Verify builtin HDR source through short/env paths.
# Registry row: SIXEL_OPTION_SCHEMA_LOADERS|g_loader_values + SIXEL_LOADER_INDEX_BUILTIN|hdr_source
# Registry binding: builtin_hdr_source
# Policy: docs/loader/color-management/README.md
# Image assertion: option-values-and-output-equivalence
set -eux
echo "1..1"
set -v

test "${HAVE_IMG2SIXEL-}" = 1 || {
    echo "ok 1 - CMS image regression # SKIP img2sixel unavailable"
    exit 0
}


# The harness names the artifact directory but does not create it.
test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"

input="${TOP_SRCDIR}/tests/data/color-numeric/cli-hdr.png"
reference="${TOP_SRCDIR}/tests/data/color-numeric/cli-hdr-source-reference.png"
artifact_dir="${ARTIFACT_LOCAL_DIR}"
short_output="${artifact_dir}/0123_loader_builtin_hdr_source_image_regression-short-$$.six"
env_output="${artifact_dir}/0123_loader_builtin_hdr_source_image_regression-env-$$.six"

short_trace=$(SIXEL_TRACE_TOPIC=suboption_contract,loader ${SIXEL_RUNTIME-} \
    "${IMG2SIXEL_PATH}" --lookup-policy=none "-Lbuiltin:Srec2020-pq:Ebuiltin!" \
    "$input" 2>&1 >"${short_output}") || {
    echo "not ok 1 - short CMS conversion failed"
    exit 0
}
case "${short_trace}" in *LSXSUB1\|*key=hdr_source\|stored=1\|binding=builtin_hdr_source\|value=5*) true ;; *) false ;; esac || {
    echo "not ok 1 - final short CMS setting is not builtin"
    exit 0
}

env_trace=$(SIXEL_TRACE_TOPIC=suboption_contract,loader ${SIXEL_RUNTIME-} \
    "${IMG2SIXEL_PATH}" --lookup-policy=none --env "SIXEL_LOADER_BUILTIN_HDR_SOURCE=rec2020-pq" "-Lbuiltin:Ebuiltin!" \
    "$input" 2>&1 >"${env_output}") || {
    echo "not ok 1 - env CMS conversion failed"
    exit 0
}
case "${env_trace}" in *LSXSUB1\|*key=hdr_source\|stored=1\|binding=builtin_hdr_source\|value=5*) true ;; *) false ;; esac || {
    echo "not ok 1 - final env CMS setting is not builtin"
    exit 0
}

cmp -s "$short_output" "$env_output" || {
    echo "not ok 1 - short and environment CMS output differ"
    exit 0
}

test -s "${short_output}" || {
    echo 'not ok 1 - empty HDR output'
    exit 0
}

# Four neutral samples use independent BT.2100 numerical expectations. Full scanning
# avoids lookup-table approximation. The sample budget is 1.275 levels for
# SIXEL percentage rounding, 0.5 for decoding, 0.5 for the PNG oracle, and
# 0.025 for floating-point color arithmetic (2.3 RGB byte levels total).
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" loader/image_values \
    "$reference" "$short_output" 'builtin:Enone!' 2.3 || {
    echo "not ok 1 - independent managed RGB samples exceed 2.3 levels"
    exit 0
}

echo "ok 1 - HDR setting reaches independently verified output"
exit 0
