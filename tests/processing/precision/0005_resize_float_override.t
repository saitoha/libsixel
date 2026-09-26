#!/bin/sh
# Policy: docs/encoder/precision/README.md
# Verify resize float override on the actual working frame.
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    precision/0005_resize_float_override 1>&2 || {
    echo "not ok 1 - resize float override"
    exit 0
}
echo "ok 1 - resize float override"
exit 0
