#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Policy: docs/loader/builtin/png.md
# Reject an APNG whose excluded default image masks a frame-count mismatch.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -Lbuiltin! \
    "${TOP_SRCDIR}/tests/data/inputs/formats/apng_invalid_libpng_fctl_after_idat.png" \
    -o/dev/null && {
    echo "not ok" 1 - "excluded-default frame-count mismatch succeeded"
    exit 0
}

echo "ok" 1 - "excluded-default frame-count mismatch is rejected"
exit 0
