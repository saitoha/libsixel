#!/bin/sh
# Verify standalone linear lookup.
# Policy: docs/encoder/output/output-colorspace.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "palette/direct_float" "25" || {
    echo "not ok 1 - standalone linear lookup"
    exit 0
}
echo "ok 1 - standalone linear lookup"
exit 0
