#!/bin/sh
# Verify none extended using fixed numeric expectations.
# Policy: docs/functionality/clamp-policy.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "dither/clamp/0004_none_extended" || {
    echo "not ok 1 - none_extended"
    exit 0
}
echo "ok 1 - none_extended"
exit 0
