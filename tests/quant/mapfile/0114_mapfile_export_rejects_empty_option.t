#!/bin/sh
# Verify an empty palette-output option value is rejected.
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
          -M '' -o/dev/null "${input_image}" 2>&1 >/dev/null) && {
    echo "not ok" 1 - "empty palette-output option unexpectedly succeeded"
    exit 0
}

test "${msg#*sixel_encoder_setopt: mapfile-output path is empty.}" \
    != "${msg}" || {
    echo "not ok" 1 - "missing empty palette-output diagnostic"
    exit 0
}

echo "ok" 1 - "empty palette-output option is rejected"
exit 0
