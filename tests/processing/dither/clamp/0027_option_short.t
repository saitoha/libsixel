#!/bin/sh
# Verify option short using fixed numeric expectations.
# Policy: docs/functionality/clamp-policy.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "dither/clamp/0027_option_short" || {
    echo "not ok 1 - option_short"
    exit 0
}
echo "ok 1 - option_short"
exit 0
