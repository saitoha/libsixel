#!/bin/sh
# Verify option nan using fixed numeric expectations.
# Policy: docs/functionality/clamp-policy.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "dither/clamp/0029_option_nan" || {
    echo "not ok 1 - option_nan"
    exit 0
}
echo "ok 1 - option_nan"
exit 0
