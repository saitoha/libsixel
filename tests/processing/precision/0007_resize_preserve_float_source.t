#!/bin/sh
# Policy: docs/functionality/precision.md
# Verify resize preserve float source on the actual working frame.
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    precision/0007_resize_preserve_float_source 1>&2 || {
    echo "not ok 1 - resize preserve float source"
    exit 0
}
echo "ok 1 - resize preserve float source"
exit 0
