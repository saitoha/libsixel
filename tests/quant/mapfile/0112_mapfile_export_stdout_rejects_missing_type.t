#!/bin/sh
# Verify palette export to standard output requires an explicit type.
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
          -M - -o/dev/null "${input_image}" 2>&1 >/dev/null) && {
    echo "not ok" 1 - "untyped palette export to stdout unexpectedly succeeded"
    exit 0
}

test "${msg#*sixel_encoder_emit_palette_output: format required for}" \
    != "${msg}" || {
    echo "not ok" 1 - "missing untyped palette-output diagnostic"
    exit 0
}

echo "ok" 1 - "palette export to stdout requires an explicit type"
exit 0
