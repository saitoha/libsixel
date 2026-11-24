#!/bin/sh
# Test-plan: docs/testing/png-chrm-arithmetic.md
# Policy: docs/loader/libpng.md
# Verify disabled CMS bypasses cHRM arithmetic.
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
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" "-Llibpng:cms_engine=none:chrm_arithmetic=512bit!" \
    "${TOP_SRCDIR}/tests/data/color-numeric/png-arithmetic-5.png" 2>&1 >/dev/null) || {
    printf '\n%s\n' "not ok 1 - conversion failed"
    printf '# %s\n' "$trace"
    exit 0
}
case "${trace}" in *PNGCHRM1\|*requested=512bit\|actual=not-applicable\|reason=cms-disabled**) true ;; *) false ;; esac || {
    printf '\n%s\n' "not ok 1 - disabled CMS bypasses cHRM arithmetic"
    printf '# %s\n' "$trace"
    exit 0
}
printf '\n%s\n' "ok 1 - disabled CMS bypasses cHRM arithmetic"
exit 0
