#!/bin/sh
# Verify output failure.
# Policy: docs/encoder/output/delta-encoding.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" delta-residual/0015_output_failure || {
    echo "not ok 1 - output failure"
    exit 0
}
echo "ok 1 - output failure"
exit 0
