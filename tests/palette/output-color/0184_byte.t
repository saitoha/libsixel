#!/bin/sh
# Byte construction remains eligible without a float palette
# Policy: docs/encoder/output/output-colorspace.md
set -eux
echo "1..1"
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "palette/retention_byte" || {
    echo "not ok 1 - Byte construction remains eligible without a float palette"
    exit 0
}
echo "ok 1 - Byte construction remains eligible without a float palette"
exit 0
