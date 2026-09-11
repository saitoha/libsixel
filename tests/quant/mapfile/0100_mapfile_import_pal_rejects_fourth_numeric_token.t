#!/bin/sh
# Verify JASC PAL import rejects a fourth numeric component token.
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

msg=$(set +xv; printf 'JASC-PAL\n0100\n1\n12 34 56 78\n' | \
          ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -L builtin! \
          -m pal-jasc:- -o/dev/null "${input_image}" 2>&1 >/dev/null) && {
    echo "not ok" 1 - "JASC PAL fourth numeric token unexpectedly succeeded"
    exit 0
}

test "${msg#*sixel_palette_parse_pal_jasc: invalid component.}" \
    != "${msg}" || {
    echo "not ok" 1 - "missing JASC PAL fourth-token diagnostic"
    exit 0
}

echo "ok" 1 - "JASC PAL fourth numeric token is rejected"
exit 0
