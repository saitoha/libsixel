#!/bin/sh
# Verify standalone din99d getter read.
# Policy: docs/encoder/output/output-colorspace.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "palette/direct_float" "64" || {
    echo "not ok 1 - standalone din99d getter read"
    exit 0
}
echo "ok 1 - standalone din99d getter read"
exit 0
