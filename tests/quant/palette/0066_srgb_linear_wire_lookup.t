#!/bin/sh
# Linear sRGB lookup uses the inverse of exact transmitted RGB101 codes.
# Policy: docs/encoder/output/output-colorspace.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "palette/0066_srgb_linear_wire_lookup" || {
    echo "not ok 1 - srgb_linear_wire_lookup"
    exit 0
}
echo "ok 1 - srgb_linear_wire_lookup"
exit 0
