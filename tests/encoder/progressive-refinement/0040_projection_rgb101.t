#!/bin/sh
# Verify projection rgb101.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "progressive/0040_projection_rgb101" || {
    printf '\n%s\n' "not ok 1 - projection rgb101"
    exit 0
}
printf '\n%s\n' "ok 1 - projection rgb101"
exit 0
