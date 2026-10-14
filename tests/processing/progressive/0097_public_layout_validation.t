#!/bin/sh
# Policy: docs/encoder/output/delta-encoding.md
# Verify public layout validation.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "progressive/0097_public_layout_validation" || {
    echo "not ok 1 - public layout validation"
    exit 0
}
echo "ok 1 - public layout validation"
exit 0
