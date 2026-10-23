#!/bin/sh
# Verify standalone linear rgb wire.
# Policy: docs/encoder/output/output-colorspace.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "palette/direct_float" "23" || {
    echo "not ok 1 - standalone linear rgb wire"
    exit 0
}
echo "ok 1 - standalone linear rgb wire"
exit 0
