#!/bin/sh
# Policy: docs/encoder/precision/README.md
# Verify auto float source on the actual working frame.
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    precision/0003_auto_float_source 1>&2 || {
    echo "not ok 1 - auto float source"
    exit 0
}
echo "ok 1 - auto float source"
exit 0
