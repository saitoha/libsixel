#!/bin/sh
# Gamma sRGB lookup uses the exact transmitted RGB101 palette.
# Policy: docs/encoder/output/output-colorspace.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "palette/0065_srgb_gamma_wire_lookup" || {
    echo "not ok 1 - srgb_gamma_wire_lookup"
    exit 0
}
echo "ok 1 - srgb_gamma_wire_lookup"
exit 0
