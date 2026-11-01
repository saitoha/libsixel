#!/bin/sh
# Verify binding generation.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "progressive/0021_binding_generation" || {
    echo "not ok 1 - binding generation"
    exit 0
}
echo "ok 1 - binding generation"
exit 0
