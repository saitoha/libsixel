#!/bin/sh
# Verify a palette-output type prefix without a path is rejected.
# Test-plan: docs/testing/mapfile-parser-coverage.md
# Policy: docs/functionality/external-palettes.md

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v

input_image="${TOP_SRCDIR}/tests/data/inputs/snake_16.png"

msg=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -bgray1 \
          -M gpl: -o/dev/null "${input_image}" 2>&1 >/dev/null) && {
    echo "not ok" 1 - "prefix-only palette output unexpectedly succeeded"
    exit 0
}

test "${msg#*sixel_encoder_emit_palette_output: invalid path.}" \
    != "${msg}" || {
    echo "not ok" 1 - "missing prefix-only palette-output diagnostic"
    exit 0
}

echo "ok" 1 - "prefix-only palette output is rejected"
exit 0
