#!/bin/sh
# Policy: docs/encoder/palette-application/working-colorspace/README.md
# Verify gamma centroid after resetting ICC clustering through img2sixel.
set -eux
echo '1..1'
set -v

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf '\n%s\n' 'ok 1 # SKIP img2sixel unavailable'
    exit 0
}
trace=$(SIXEL_TRACE_TOPIC=loader ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --threading-policy=greedy:global_budget=1 --precision-policy=float32 \
    -7 -Lbuiltin! -Ugamma -_none -~none -Fnone -trgb \
    -Qkmeans:seed=1 -p2 "-Xicc:${TOP_SRCDIR}/tests/data/color-numeric/display-p3.icc" -Xauto -Wgamma -aoff -dnone \
    --binning-policy=none --sampling-policy=full-frame 2>&1 <<'PNM'
P3
3 1
255
223 31 25  72 57 29  16 169 221
PNM
) || {
    printf '%s\n' "$trace" >&2
    printf '\n%s\n' 'not ok 1 - gamma centroid after resetting ICC clustering conversion'
    exit 0
}
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" cli/0036_cli_color_wire \
    pnm-cluster-gamma "$trace" || {
    printf '\n%s\n' 'not ok 1 - gamma centroid after resetting ICC clustering'
    exit 0
}
printf '\n%s\n' 'ok 1 - gamma centroid after resetting ICC clustering'
exit 0
