#!/bin/sh
# Verify PNM truncated-ASCII policy through short and environment paths.
# Policy: docs/misc/platforms/openvms.md
# Coverage: OV-13
# Registry row: SIXEL_OPTION_SCHEMA_LOADERS|g_loader_values + SIXEL_LOADER_INDEX_BUILTIN|pnm_truncated_ascii
# Registry binding: builtin_pnm_truncated_ascii
# Exact image reference: literal-pnm

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v

test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"
# GNV file utilities can treat the leading Unix directory as a logical
# device. Keep artifact names relative to the directory opened by the shell.
artifact_dir="${ARTIFACT_LOCAL_DIR}"
cd "${artifact_dir}"

input_image="${TOP_SRCDIR}/tests/data/inputs/formats/pnm-truncated-ascii-2x1.ppm"
short_output="0106-pnm_truncated_ascii-short-$$.six"
env_output="0106-pnm_truncated_ascii-env-$$.six"

short_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    "-Lbuiltin:Y1!" "${input_image}" \
    2>&1 >"${short_output}") || {
    echo "not ok" 1 - "pnm_truncated_ascii short conversion failed"
    exit 0
}

case "${short_trace}" in *LSXSUB1\|*key=pnm_truncated_ascii\|stored=1\|binding=builtin_pnm_truncated_ascii\|value=1**) true ;; *) false ;; esac || {
    echo "not ok" 1 - "pnm_truncated_ascii short value was not stored"
    exit 0
}

env_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env "SIXEL_LOADER_PNM_ALLOW_TRUNCATED_ASCII=1" \
    "-Lbuiltin!" "${input_image}" \
    2>&1 >"${env_output}") || {
    echo "not ok" 1 - "pnm_truncated_ascii environment conversion failed"
    exit 0
}

case "${env_trace}" in *LSXSUB1\|*key=pnm_truncated_ascii\|stored=1\|binding=builtin_pnm_truncated_ascii\|value=1**) true ;; *) false ;; esac || {
    echo "not ok" 1 - "pnm_truncated_ascii environment value was not stored"
    exit 0
}

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    "-Lbuiltin:Y0!" "${input_image}" \
    >/dev/null && {
    echo "not ok" 1 - "pnm_truncated_ascii disabled control unexpectedly succeeded"
    exit 0
}

cmp -s "${short_output}" "${env_output}" || {
    echo "not ok" 1 - "pnm_truncated_ascii short and env output differ"
    exit 0
}

# The absent second pixel is independently specified as black. Encode that
# complete literal image through the strict path before comparing exact
# bytes. A short GNV pipeline can retain its writer side and withhold EOF.
reference_input="0106-pnm-complete-reference-$$.ppm"
reference_output="0106-pnm-truncated-reference-$$.six"
printf 'P3\n2 1\n255\n255 0 0 0 0 0\n' >"${reference_input}"
${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" "-Lbuiltin:Y0!" \
    "${reference_input}" \
    >"${reference_output}" || {
    echo "not ok 1 - complete literal PNM reference conversion failed"
    exit 0
}
cmp -s "${reference_output}" "${short_output}" || {
    echo "not ok 1 - truncated PNM differs from its literal pixel reference"
    exit 0
}

echo "ok" 1 - "pnm_truncated_ascii preserves exact image output"
exit 0
