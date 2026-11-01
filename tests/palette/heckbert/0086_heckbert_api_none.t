#!/bin/sh
# Verify the api none palette through the public dither API.
# Policy: docs/encoder/palette-construction/quantization/README.md
set -eux
echo "1..1"
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "palette/heckbert_policy/api_none" || {
    echo "not ok 1 - api_none"
    exit 0
}
echo "ok 1 - api_none"
exit 0
