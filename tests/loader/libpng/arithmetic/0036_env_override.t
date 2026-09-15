#!/bin/sh
# Test-plan: docs/testing/png-chrm-arithmetic.md
# Policy: docs/loader/libpng.md
# Verify CLI arithmetic overrides the environment.
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
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --env SIXEL_LOADER_LIBPNG_CHRM_ARITHMETIC=512bit "-Llibpng:cms_engine=builtin:chrm_arithmetic=double!" \
    "${TOP_SRCDIR}/tests/data/color-numeric/png-arithmetic-1.png" 2>&1 >/dev/null) || {
    echo "not ok 1 - conversion failed"
    printf '# %s\n' "$trace"
    exit 0
}
test "${trace#*PNGCHRM1|*requested=double|selected=double|actual=double|reason=forced*}" != "$trace" || {
    echo "not ok 1 - CLI arithmetic overrides the environment"
    printf '# %s\n' "$trace"
    exit 0
}
echo "ok 1 - CLI arithmetic overrides the environment"
exit 0
