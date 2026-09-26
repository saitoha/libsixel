#!/bin/sh
# Policy: docs/loader/color-management/README.md
# All positive curv exponent words map to independent integer and byte values.
set -eux
echo "1..1"
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" cms/0146_curv_encoding || {
    echo "not ok 1 - curv integer and byte encoding"
    exit 0
}
echo "ok 1 - curv integer and byte encoding"
exit 0
