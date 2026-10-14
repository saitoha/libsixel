#!/bin/sh
# Policy: docs/encoder/output/delta-encoding.md
# Verify ignored delay final repair.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "progressive/0116_ignored_delay_final_repair" || {
    echo "not ok 1 - ignored delay final repair"
    exit 0
}
echo "ok 1 - ignored delay final repair"
exit 0
