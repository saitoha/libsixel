#!/bin/sh
# Verify LSO2 cielab lightness with unchanged extended chroma using fixed numeric expectations.
# Policy: docs/encoder/palette-application/clamp-policy.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "dither/clamp/0033_lso2_cielab_lightness" || {
    echo "not ok 1 - lso2_cielab_lightness"
    exit 0
}
echo "ok 1 - lso2_cielab_lightness"
exit 0
