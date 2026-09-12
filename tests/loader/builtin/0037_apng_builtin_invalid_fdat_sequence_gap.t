#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Policy: docs/loader/builtin/png.md
# TAP test: builtin loader rejects a late APNG fdAT sequence gap.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}


echo "1..1"
set -v

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -Lbuiltin! \
    "${TOP_SRCDIR}/tests/data/inputs/formats/apng_invalid_libpng_fdat_sequence_gap.png" \
    -o/dev/null && {
    echo "not ok" 1 - "APNG fdAT sequence gap unexpectedly succeeded"
    exit 0
}

echo "ok" 1 - "APNG fdAT sequence gap is rejected"
exit 0
