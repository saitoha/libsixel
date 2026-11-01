#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# TAP test confirming builtin HDR tonemap output avoids black collapse.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}


echo "1..1"
set -v
test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"

input_hdr="${TOP_SRCDIR}/tests/data/inputs/formats/stbi_midtones.hdr"
output_sixel="${ARTIFACT_LOCAL_DIR}/builtin-hdr-tonemap-reinhard.six"

SIXEL_LOADER_HDR_TONEMAP=reinhard \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -Lbuiltin:cms_engine=none! "${input_hdr}" >"${output_sixel}" || {
    echo "not ok" 1 - "builtin loader failed to decode HDR with tonemap=reinhard"
    exit 0
}

# Observe actual decoded sample variation. An unavailable spatial metric
# cannot be evidence that the image differs from a solid black/white frame.
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" loader/image_sample_range \
    "${output_sixel}" 1 || {
    echo "not ok 1 - HDR output lost its RGB sample range"
    exit 0
}

echo "ok" 1 - "builtin loader decodes HDR with tonemap=reinhard and avoids black output collapse"
exit 0
