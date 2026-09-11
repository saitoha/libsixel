#!/bin/sh
# Verify untyped standard-input mapfiles are rejected as ambiguous.
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

msg=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -L builtin! \
          -m - -o/dev/null "${input_image}" 2>&1 <<'PAL'
JASC-PAL
0100
1
12 34 56
PAL
) && {
    echo "not ok" 1 - "untyped standard-input mapfile unexpectedly succeeded"
    exit 0
}

test "${msg#*sixel_prepare_specified_palette: format required for}" != "${msg}" || {
    echo "not ok" 1 - "missing untyped standard-input diagnostic"
    exit 0
}

echo "ok" 1 - "untyped standard-input mapfile is rejected"
exit 0
