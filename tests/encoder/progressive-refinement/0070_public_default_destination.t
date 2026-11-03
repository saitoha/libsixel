#!/bin/sh
# Policy: docs/encoder/output/delta-encoding.md
# Policy: docs/build/test-harness.md
# Policy: docs/misc/platforms/msvc.md
# Coverage: MSVC-03
# Verify public default destination.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "progressive/0096_public_default_destination" || {
    echo "not ok 1 - public default destination"
    exit 0
}
echo "ok 1 - public default destination"
exit 0
