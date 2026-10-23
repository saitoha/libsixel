#!/bin/sh
# Verify standalone din99d output overflow recovery.
# Policy: docs/encoder/output/output-colorspace.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "palette/direct_float" "75" || {
    echo "not ok 1 - standalone din99d output overflow recovery"
    exit 0
}
echo "ok 1 - standalone din99d output overflow recovery"
exit 0
