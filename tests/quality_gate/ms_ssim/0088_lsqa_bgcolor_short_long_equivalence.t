#!/bin/sh
# Verify -B/--bgcolor accepts img2sixel-compatible color syntax.

set -eux


printf '1..1\n'
set -v

image_ref="${TOP_SRCDIR}/tests/data/inputs/snake_64.bmp"
image_out="${TOP_SRCDIR}/tests/data/inputs/snake_64.six"
value_short=$(${SIXEL_RUNTIME-} "${LSQA_PATH}" -m MS-SSIM -B "#112233" \
    "${image_ref}" "${image_out}") || {
    echo "not ok" 1 - "lsqa -B #rrggbb should be accepted"
    exit 0
}
value_long=$(${SIXEL_RUNTIME-} "${LSQA_PATH}" \
    "${image_ref}" "${image_out}" --bgcolor="rgb:11/22/33" -m MS-SSIM) || {
    echo "not ok" 1 - "lsqa --bgcolor should work after positional args"
    exit 0
}

test "${value_short}" = "${value_long}" || {
    echo "not ok" 1 - "short and long bgcolor forms changed output"
    exit 0
}

echo "ok" 1 - "lsqa accepts -B and --bgcolor syntax"
exit 0
