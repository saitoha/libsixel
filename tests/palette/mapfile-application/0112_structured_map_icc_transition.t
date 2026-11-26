#!/bin/sh
# Policy: docs/encoder/palette-construction/external-palettes.md
# A non-reopenable structured palette survives both ICC option transitions.
set -eux
echo '1..1'
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/map_cluster/0112_structured_map_icc_transition \
    "${TOP_SRCDIR}/tests/data/color-numeric/display-p3.icc" <<'PAL' || {
JASC-PAL
0100
2
255 0 0
0 0 255
PAL
    printf '\n%s\n' 'not ok 1 - fixed palette across clustering changes'
    exit 0
}
printf '\n%s\n' 'ok 1 - fixed palette across clustering changes'
exit 0
