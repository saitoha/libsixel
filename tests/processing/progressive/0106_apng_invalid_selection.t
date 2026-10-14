#!/bin/sh
# Policy: docs/encoder/output/delta-encoding.md
# Verify apng invalid selection.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "progressive/0106_apng_invalid_selection" || {
    echo "not ok 1 - apng invalid selection"
    exit 0
}
echo "ok 1 - apng invalid selection"
exit 0
