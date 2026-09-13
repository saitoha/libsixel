#!/bin/sh
# Verify invalid rgb definition.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "cms/0008_invalid_rgb_definition" || {
    echo "not ok 1 - invalid rgb definition"
    exit 0
}

echo "ok 1 - invalid rgb definition"
exit 0
