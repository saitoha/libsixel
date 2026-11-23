#!/bin/sh
# TODO: In output-target mode only bypass explicit diffusion when the
# generated palette actually reproduces the targets. Two grays fit two
# slots, but the P3 wire codes on the neutral axis are 50% and 51%.
# With FS, .5062 + (.497 - .50)*7/16 < .505. Both pixels must use 50%.
# Keep palette snap enabled and the continuous pixel target unsnapped.
set -eux
echo '1..1'
set -v

test "${HAVE_IMG2SIXEL-}" = 1 || {
    echo 'ok 1 # SKIP img2sixel unavailable'
    exit 0
}
trace=$(printf 'P6\n2 1\n65535\n\177\073\177\073\177\073\201\226\201\226\201\226' |
    SIXEL_PALETTE_TARGET=output SIXEL_TRACE_TOPIC=dither_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --threading-policy=greedy:B1 \
    --precision-policy=float32 --sampling-policy=full-frame \
    --binning-policy=none -7 -Lbuiltin! -Xgamma -Wgamma \
    -U "icc:P${TOP_SRCDIR}/tests/data/color-numeric/display-p3.icc" \
    -_nearest -~none -aoff -Fnone -0none -trgb -Qkmeans:seed=1 -p2 \
    -dfs:scan=raster - 2>&1) || {
    echo 'not ok 1 - generated ICC palette encode failed'
    exit 0
}
status=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" palette/icc_output/0001_icc_output \
    exact-target "$trace" || status=$?
test "$status" -ne 42 || {
    echo 'not ok 1 - two source colors skip FS despite a register residual # TODO known result'
    exit 0
}
test "$status" -ne 0 || {
    echo 'not ok 1 - expected behavior now works; promote this test'
    exit 0
}
echo 'not ok 1 - unexpected fixture, execution or pixel result'
exit 0
