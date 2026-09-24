#!/bin/sh
# Verify soft vector using fixed numeric expectations.
# Policy: docs/functionality/clamp-policy.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "dither/clamp/0001_soft_vector" || {
    echo "not ok 1 - soft_vector"
    exit 0
}
echo "ok 1 - soft_vector"
exit 0
