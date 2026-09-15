#!/bin/sh
# Test-plan: docs/testing/png-chrm-arithmetic.md
# Policy: docs/loader/libpng.md
# Verify exactly collinear cHRM remains metadata fallback.
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
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" "-Llibpng:cms_engine=builtin:chrm_arithmetic=512bit!" \
    "${TOP_SRCDIR}/tests/data/color-numeric/png-chrm-16-rgb-collinear-0-d65-gama-chrm.png" 2>&1 >/dev/null) || {
    echo "not ok 1 - conversion failed"
    printf '# %s\n' "$trace"
    exit 0
}
test "${trace#*PNGCHRM1|*requested=512bit|actual=not-applicable|reason=undefined-chrm*}" != "$trace" || {
    echo "not ok 1 - exactly collinear cHRM remains metadata fallback"
    printf '# %s\n' "$trace"
    exit 0
}
echo "ok 1 - exactly collinear cHRM remains metadata fallback"
exit 0
