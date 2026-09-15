#!/bin/sh
# Test-plan: docs/testing/png-chrm-arithmetic.md
# Policy: docs/loader/libpng.md
# Verify omitted arithmetic selects auto.
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
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --env SIXEL_LOADER_LIBPNG_CHRM_ARITHMETIC= "-Llibpng:cms_engine=builtin!" \
    "${TOP_SRCDIR}/tests/data/color-numeric/png-arithmetic-5.png" 2>&1 >/dev/null) || {
    echo "not ok 1 - conversion failed"
    printf '# %s\n' "$trace"
    exit 0
}
test "${trace#*PNGCHRM1|*requested=auto|selected=512bit|actual=512bit|reason=small-area+cone*}" != "$trace" || {
    echo "not ok 1 - omitted arithmetic selects auto"
    printf '# %s\n' "$trace"
    exit 0
}
echo "ok 1 - omitted arithmetic selects auto"
exit 0
