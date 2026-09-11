#!/bin/sh
# Verify GPL export writes all 256 canonical rows exactly.
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
input_palette="${TOP_SRCDIR}/tests/data/inputs/mapfile/pal-256-valid.pal"
expected_palette="${TOP_SRCDIR}/tests/data/inputs/mapfile/gpl-256-valid.gpl"
actual_palette="${ARTIFACT_LOCAL_DIR}/actual.gpl"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -m pal-jasc:"${input_palette}" \
    -M gpl:"${actual_palette}" -o/dev/null "${input_image}" || {
    echo "not ok" 1 - "256-color GPL export failed"
    exit 0
}

cmp -s "${actual_palette}" "${expected_palette}" || {
    echo "not ok" 1 - "GPL export did not write 256 canonical rows"
    exit 0
}

echo "ok" 1 - "GPL export writes all 256 canonical rows"
exit 0
