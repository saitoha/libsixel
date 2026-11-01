#!/bin/sh
# Verify option minimum using fixed numeric expectations.
# Policy: docs/encoder/palette-application/clamp-policy.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "dither/clamp/0030_option_minimum" || {
    echo "not ok 1 - option_minimum"
    exit 0
}
echo "ok 1 - option_minimum"
exit 0
