#!/bin/sh
# Policy: docs/encoder/output/delta-encoding.md
# Verify prepare force policy.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "progressive/0067_prepare_force_policy" || {
    echo "not ok 1 - prepare force policy"
    exit 0
}
echo "ok 1 - prepare force policy"
exit 0
