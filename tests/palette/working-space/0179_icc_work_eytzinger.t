#!/bin/sh
# Policy: docs/encoder/palette-application/working-colorspace/README.md
# Verify eytzinger ICC nearest fixed-palette colors through img2sixel.
set -eux
echo '1..1'
set -v

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf '\n%s\n' 'ok 1 # SKIP img2sixel unavailable'
    exit 0
}
trace=$(SIXEL_TRACE_TOPIC=loader ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --threading-policy=greedy:global_budget=1 --precision-policy=float32 \
    -7 -Lbuiltin! -Usrgb -_none -~eytzinger -Fnone -trgb \
    -bxterm16 -Xsrgb "-Wicc:${TOP_SRCDIR}/tests/data/color-numeric/display-p3.icc" -0none -dnone 2>&1 <<'PNM'
P3
2 1
255
76 190 42  75 218 173
PNM
) || {
    printf '%s\n' "$trace" >&2
    printf '\n%s\n' 'not ok 1 - eytzinger ICC nearest fixed-palette colors conversion'
    exit 0
}
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" cli/0036_cli_color_wire \
    pnm-lookup-icc "$trace" || {
    printf '\n%s\n' 'not ok 1 - eytzinger ICC nearest fixed-palette colors'
    exit 0
}
printf '\n%s\n' 'ok 1 - eytzinger ICC nearest fixed-palette colors'
exit 0
