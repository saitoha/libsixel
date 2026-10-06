#!/bin/sh
# Environment-selected delta also rejects monochrome.
# Policy: docs/encoder/output/delta-encoding.md
set -eux

echo "1..1"
set -v

SIXEL_UPDATE_POLICY="delta" ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/delta_native/0203_delta_mono_environment || {
    echo "not ok 1 - mono environment"
    exit 0
}
echo "ok 1 - mono environment"
exit 0
