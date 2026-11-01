#!/bin/sh
# Policy: docs/concepts/pixelformat.md
# Verify frame continuous alpha mask reject.
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "frame/0239_alpha" 16 || {
    echo "not ok 1 - frame continuous alpha mask reject"
    exit 0
}
echo "ok 1 - frame continuous alpha mask reject"
exit 0
