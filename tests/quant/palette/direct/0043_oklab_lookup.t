#!/bin/sh
# Verify standalone oklab lookup.
# Policy: docs/encoder/output/output-colorspace.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "palette/direct_float" "43" || {
    echo "not ok 1 - standalone oklab lookup"
    exit 0
}
echo "ok 1 - standalone oklab lookup"
exit 0
