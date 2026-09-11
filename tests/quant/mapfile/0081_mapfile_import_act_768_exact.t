#!/bin/sh
# Verify a trailer-free 768-byte ACT imports all 256 ordered entries.
# Test-plan: docs/testing/mapfile-parser-coverage.md
# Policy: docs/functionality/external-palettes.md

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v
test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"

input_image="${TOP_SRCDIR}/tests/data/inputs/snake_16.png"
input_palette="${TOP_SRCDIR}/tests/data/inputs/mapfile/act-black-white-256.act"
actual_palette="${ARTIFACT_LOCAL_DIR}/actual.act"
expected_palette="${TOP_SRCDIR}/tests/data/inputs/mapfile/act-black-white-count-256.act"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -m act:"${input_palette}" \
    -M act:"${actual_palette}" -o /dev/null "${input_image}" || {
    echo "not ok" 1 - "768-byte ACT palette import failed"
    exit 0
}

cmp -s "${actual_palette}" "${expected_palette}" || {
    echo "not ok" 1 - "768-byte ACT did not import as 256 ordered entries"
    exit 0
}

echo "ok" 1 - "768-byte ACT imports all 256 entries in order"
exit 0
