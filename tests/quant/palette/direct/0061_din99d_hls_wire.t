#!/bin/sh
# Verify standalone din99d hls wire.
# Policy: docs/encoder/output/output-colorspace.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "palette/direct_float" "62" || {
    echo "not ok 1 - standalone din99d hls wire"
    exit 0
}
echo "ok 1 - standalone din99d hls wire"
exit 0
