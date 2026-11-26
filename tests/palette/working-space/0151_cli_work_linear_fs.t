#!/bin/sh
# Policy: docs/encoder/palette-application/working-colorspace/README.md
# Verify linear fixed-palette raster FS feedback through img2sixel.
set -eux
echo '1..1'
set -v

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf '\n%s\n' 'ok 1 # SKIP img2sixel unavailable'
    exit 0
}
trace=$(SIXEL_TRACE_TOPIC=loader ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --threading-policy=greedy:global_budget=1 --precision-policy=float32 \
    -7 -Lbuiltin! -Usrgb -_none -~none -Fnone -trgb \
    -bxterm16 -Xsrgb -Wsrgb-linear -0none -dfs:scan=raster 2>&1 <<'PNM'
P3
2 1
255
47 195 161  70 56 192
PNM
) || {
    printf '%s\n' "$trace" >&2
    printf '\n%s\n' 'not ok 1 - linear fixed-palette raster FS feedback conversion'
    exit 0
}
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" cli/0036_cli_color_wire \
    pnm-fs-linear "$trace" || {
    printf '\n%s\n' 'not ok 1 - linear fixed-palette raster FS feedback'
    exit 0
}
printf '\n%s\n' 'ok 1 - linear fixed-palette raster FS feedback'
exit 0
