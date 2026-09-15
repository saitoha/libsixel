#!/bin/sh
# Test-plan: docs/testing/png-chrm-arithmetic.md
# Policy: docs/loader/libpng.md
# Verify lsqa forwards its loader arithmetic setting.
set -eux
test "${HAVE_LIBPNG-}" = 1 || {
    echo "1..0 # SKIP libpng is unavailable"
    exit 0
}
test "${HAVE_IMG2SIXEL-}" = 1 || {
    echo "1..0 # SKIP img2sixel is unavailable"
    exit 0
}
echo "1..1"
set -v

trace=$(set +xv; SIXEL_TRACE_TOPIC=loader \
    ${SIXEL_RUNTIME-} "${LSQA_PATH}" \
    "-Llibpng:cms_engine=builtin:A512bit!" -b MS-SSIM:0.98 \
    "${TOP_SRCDIR}/tests/data/inputs/snake_16.png" \
    "${TOP_SRCDIR}/tests/data/inputs/snake_16.png" 2>&1) || {
    echo "not ok 1 - lsqa arithmetic conversion failed"
    printf '# %s\n' "$trace"
    exit 0
}
test "${trace#*PNGCHRM1|*requested=512bit|selected=512bit|actual=identity*}" != "$trace" || {
    echo "not ok 1 - lsqa did not forward forced cHRM arithmetic"
    exit 0
}
echo "ok 1 - lsqa forwards forced cHRM arithmetic"
exit 0
