#!/bin/sh
# Invalid low-level updates preserve both settings.
# Policy: docs/functionality/clamp-policy.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "dither/clamp/0036_api_invalid" || {
    echo "not ok 1 - api_invalid"
    exit 0
}
echo "ok 1 - api_invalid"
exit 0
