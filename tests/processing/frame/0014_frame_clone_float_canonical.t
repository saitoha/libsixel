#!/bin/sh
# Policy: docs/concepts/pixelformat-precision.md
# Verify frame clone float canonical.
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "frame/0014_clone_float_canonical" || {
    echo "not ok 1 - frame clone float canonical"
    exit 0
}
echo "ok 1 - frame clone float canonical"
exit 0
