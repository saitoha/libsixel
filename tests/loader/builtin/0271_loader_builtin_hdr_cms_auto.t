#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# TAP test confirming builtin HDR decode succeeds with cms_engine=auto.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}


echo "1..1"
set -v
test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"

input_hdr="${TOP_SRCDIR}/tests/data/inputs/formats/stbi_minimal.hdr"
output_auto="${ARTIFACT_LOCAL_DIR}/builtin-hdr-cms-auto.six"
output_none="${ARTIFACT_LOCAL_DIR}/builtin-hdr-cms-none-reference.six"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -Lbuiltin:cms_engine=auto! "${input_hdr}" >"${output_auto}" || {
    echo "not ok" 1 - "builtin loader failed to decode HDR with cms=auto"
    exit 0
}

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -Lbuiltin:cms_engine=none! "${input_hdr}" >"${output_none}" || {
    echo "not ok" 1 - "builtin loader failed to decode HDR reference with cms=none"
    exit 0
}

cmp -s "${output_none}" "${output_auto}" || {
    echo "not ok 1 - HDR CMS auto must match the unprofiled reference bytes"
    exit 0
}

# Observe actual decoded sample variation. An unavailable spatial metric
# cannot be evidence that the image differs from a solid black/white frame.
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" loader/image_sample_range \
    "${output_auto}" 1 || {
    echo "not ok 1 - HDR output lost its RGB sample range"
    exit 0
}

echo "ok" 1 - "builtin loader decodes HDR with cms=auto, matches cms=none, and avoids black output collapse"
exit 0
