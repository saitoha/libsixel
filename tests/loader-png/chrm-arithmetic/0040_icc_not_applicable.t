#!/bin/sh
# Test-plan: docs/testing/png-chrm-arithmetic.md
# Policy: docs/loader/libpng.md
# Verify selected ICC bypasses cHRM arithmetic.
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
    "${TOP_SRCDIR}/tests/data/color-numeric/png-chrm-16-rgb-zero-icc-chrm.png" 2>&1 >/dev/null) || {
    echo "not ok 1 - conversion failed"
    printf '# %s\n' "$trace"
    exit 0
}
case "${trace}" in *PNGCHRM1\|*requested=512bit\|actual=not-applicable\|reason=icc-selected**) true ;; *) false ;; esac || {
    echo "not ok 1 - selected ICC bypasses cHRM arithmetic"
    printf '# %s\n' "$trace"
    exit 0
}
echo "ok 1 - selected ICC bypasses cHRM arithmetic"
exit 0
