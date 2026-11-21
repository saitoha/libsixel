#!/bin/sh
# Reject an image taller than the PNG writer's height limit.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" writer/png/0042_height_limit || {
    echo "not ok 1 - PNG writer rejects height above the limit"
    exit 0
}
echo "ok 1 - PNG writer rejects height above the limit"
exit 0
