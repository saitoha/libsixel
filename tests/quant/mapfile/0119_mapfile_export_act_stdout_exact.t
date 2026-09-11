#!/bin/sh
# Verify ACT palette export writes its exact binary layout to stdout.
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
actual_palette="${ARTIFACT_LOCAL_DIR}/actual.act"
expected_palette="${TOP_SRCDIR}/tests/data/inputs/mapfile/act-black-white-count-2.act"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -bgray1 \
    -M act:- -o/dev/null "${input_image}" >"${actual_palette}" || {
    echo "not ok" 1 - "ACT palette export to stdout failed"
    exit 0
}

cmp -s "${actual_palette}" "${expected_palette}" || {
    echo "not ok" 1 - "ACT stdout palette layout changed"
    exit 0
}

echo "ok" 1 - "ACT palette export to stdout has exact layout"
exit 0
