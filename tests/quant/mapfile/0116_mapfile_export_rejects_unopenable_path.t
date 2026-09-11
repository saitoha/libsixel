#!/bin/sh
# Verify palette export reports a file-open failure.
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
output_palette="${ARTIFACT_LOCAL_DIR}/missing/palette.gpl"

msg=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -bgray1 \
          -M "${output_palette}" -o/dev/null "${input_image}" \
          2>&1 >/dev/null) && {
    echo "not ok" 1 - "unopenable palette-output path unexpectedly succeeded"
    exit 0
}

test "${msg#*sixel_encoder_emit_palette_output: failed to open file.}" \
    != "${msg}" || {
    echo "not ok" 1 - "missing palette-output open-failure diagnostic"
    exit 0
}

echo "ok" 1 - "unopenable palette-output path is rejected"
exit 0
