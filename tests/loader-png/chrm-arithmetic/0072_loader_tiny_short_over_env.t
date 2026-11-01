#!/bin/sh
# Test-plan: docs/testing/png-chrm-arithmetic.md
# Policy: docs/loader/libpng.md
# Verify loader shared resolver short_over_env tiny frame bits.
set -eux
test "${HAVE_LIBPNG-}" = 1 || {
    echo "1..0 # SKIP libpng is unavailable"
    exit 0
}
echo "1..1"
set -v

result=0
trace=$(set +xv; SIXEL_TRACE_TOPIC=loader ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" --env "SIXEL_LOADER_LIBPNG_CHRM_ARITHMETIC=512bit" loader/0210_png_arith_loader 8 "libpng:cms_engine=builtin:cms_target=linear:prefer_8bit=0:Adouble!" 1 2>&1) || result=$?
test "$result" -ne 77 || {
    echo "ok 1 # SKIP required arithmetic representation is unavailable"
    exit 0
}
test "$result" -eq 0 || {
    echo "not ok 1 - loader shared resolver short_over_env tiny frame bits"
    exit 0
}
case "${trace}" in *PNGCHRM1\|role=image\|requested=double\|selected=double\|actual=double\|reason=forced**) true ;; *) false ;; esac || {
    echo "not ok 1 - selected arithmetic path differs"
    printf '# %s\n' "$trace"
    exit 0
}
echo "ok 1 - loader shared resolver short_over_env tiny frame bits"
exit 0
