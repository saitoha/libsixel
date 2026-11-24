#!/bin/sh
# Verify the api initial palette through the public dither API.
# Policy: docs/encoder/palette-construction/quantization/README.md
set -eux
echo "1..1"
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "palette/heckbert_policy/api_initial" || {
    printf '\n%s\n' "not ok 1 - api_initial"
    exit 0
}
printf '\n%s\n' "ok 1 - api_initial"
exit 0
