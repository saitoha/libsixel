#!/bin/sh
# Policy: docs/encoder/output/output-colorspace.md
# Verify the emitted ICC palette against independent working-space values.
set -eux
echo '1..1'
set -v

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf '\n%s\n' 'ok 1 # SKIP img2sixel unavailable'
    exit 0
}
data="${TOP_SRCDIR}/tests/data/color-numeric"
export SIXEL_LOADER_CMS_BPC=0 SIXEL_LOADER_CMS_RENDERING_INTENT=relative
trace=$(SIXEL_TRACE_TOPIC=loader ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --threading-policy=greedy:global_budget=1 --precision-policy=float32 \
    -7 -Lbuiltin:Ebuiltin! -U "icc:P${data}/output-bpc.icc:bpc=0" \
    -trgb -0none -dnone \
    -Xoklab -Wsrgb-linear -m "gpl:${data}/icc-register-fixed.gpl" \
    "${data}/icc-register-bpc0.png" 2>&1) || {
    printf '%s\n' "$trace" >&2
    printf '\n%s\n' 'not ok 1 - encode projected ICC palette'
    exit 0
}
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" cli/0036_cli_color_wire \
    icc-register-nearest "$trace" || {
    printf '\n%s\n' 'not ok 1 - projected ICC palette colors and indices'
    exit 0
}
printf '\n%s\n' 'ok 1 - projected ICC palette colors and indices'
exit 0
