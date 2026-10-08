#!/bin/sh
# Verify standalone oklab rgb wire.
# Policy: docs/encoder/output/output-colorspace.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "palette/direct_float" "41" || {
    echo "not ok 1 - standalone oklab rgb wire"
    exit 0
}
echo "ok 1 - standalone oklab rgb wire"
exit 0
