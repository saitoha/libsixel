#!/bin/sh
# Verify the api auto palette through the public dither API.
# Policy: docs/encoder/palette-construction/quantization/README.md
set -eux
echo "1..1"
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "palette/heckbert_policy/api_auto" || {
    echo "not ok 1 - api_auto"
    exit 0
}
echo "ok 1 - api_auto"
exit 0
