#!/bin/sh
# Policy: docs/encoder/output/delta-encoding.md
# Verify public unsupported controls.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "progressive/0099_public_unsupported_controls" || {
    echo "not ok 1 - public unsupported controls"
    exit 0
}
echo "ok 1 - public unsupported controls"
exit 0
