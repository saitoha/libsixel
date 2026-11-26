#!/bin/sh
# Policy: docs/encoder/palette-application/working-colorspace/README.md
# Check ICC center representatives and undithered linear RGB lookup.
set -eux
echo '1..1'
set -v

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf '\n%s\n' 'ok 1 # SKIP img2sixel unavailable'
    exit 0
}
trace=$(SIXEL_TRACE_TOPIC=loader,palette_contract ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --threading-policy=greedy:global_budget=1 --precision-policy=float32 \
    -7 -Lbuiltin! -Ugamma -_none -~none -Fnone -trgb \
    -Qcenter:algo=hybrid:seed=1:restarts=8:init_seeds=8:space_policy=legacy \
    -p2 "-Xicc:${TOP_SRCDIR}/tests/data/color-numeric/display-p3.icc" \
    -Wlinear -aoff -dnone \
    --binning-policy=none --sampling-policy=full-frame \
    "${TOP_SRCDIR}/tests/data/color-numeric/icc-center-samples.ppm" 2>&1) || {
    printf '%s\n' "$trace" >&2
    printf '\n%s\n' 'not ok 1 - center ICC encode'
    exit 0
}
case "$trace" in *'LSXBPS1|quantizer_requested=4|quantizer_effective=4|'*) true ;; *) false ;; esac || {
    printf '\n%s\n' 'not ok 1 - expected center solver'
    exit 0
}
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" cli/0036_cli_color_wire \
    icc-center-linear "$trace" || {
    printf '\n%s\n' 'not ok 1 - center ICC representatives and linear lookup pixels'
    exit 0
}
printf '\n%s\n' 'ok 1 - center ICC representatives and linear lookup pixels'
exit 0
