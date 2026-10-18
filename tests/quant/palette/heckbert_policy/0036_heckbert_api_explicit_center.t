#!/bin/sh
# Verify the api explicit center palette through the public dither API.
# Policy: docs/encoder/palette-construction/quantization/README.md
set -eux
echo "1..1"
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "palette/heckbert_policy/api_explicit_center" || {
    echo "not ok 1 - api_explicit_center"
    exit 0
}
echo "ok 1 - api_explicit_center"
exit 0
