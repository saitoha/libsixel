#!/bin/sh
# Policy: docs/encoder/output/delta-encoding.md
# Verify step pre admission priority.
set -eux

echo "1..1"
set -v

set +e
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "progressive/0093_step_pre_admission_priority"
result=$?
set -e
test "$result" -ne 77 || {
    echo "ok 1 - step pre admission priority # SKIP thread support disabled"
    exit 0
}
test "$result" -eq 0 || {
    echo "not ok 1 - step pre admission priority"
    exit 0
}
echo "ok 1 - step pre admission priority"
exit 0
