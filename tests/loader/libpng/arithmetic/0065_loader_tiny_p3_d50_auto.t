#!/bin/sh
# Test-plan: docs/testing/png-chrm-arithmetic.md
# Policy: docs/loader/libpng.md
# Verify real loader p3_d50 auto tiny frame bits.
set -eux
test "${HAVE_LIBPNG-}" = 1 || {
    echo "1..0 # SKIP libpng is unavailable"
    exit 0
}
echo "1..1"
set -v

result=0
trace=$(set +xv; SIXEL_TRACE_TOPIC=loader ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" loader/0210_png_arith_loader 9 "libpng:cms_engine=builtin:cms_target=linear:prefer_8bit=0:chrm_arithmetic=auto!" 1 2>&1) || result=$?
test "$result" -ne 77 || {
    echo "ok 1 # SKIP required arithmetic representation is unavailable"
    exit 0
}
test "$result" -eq 0 || {
    echo "not ok 1 - real loader p3_d50 auto tiny frame bits"
    exit 0
}
test "${trace#*PNGCHRM1|role=image|requested=auto|selected=double|actual=double|reason=ordinary*}" != "$trace" || {
    echo "not ok 1 - selected arithmetic path differs"
    printf '# %s\n' "$trace"
    exit 0
}
echo "ok 1 - real loader p3_d50 auto tiny frame bits"
exit 0
