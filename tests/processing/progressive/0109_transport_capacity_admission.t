#!/bin/sh
# Policy: docs/encoder/output/delta-encoding.md
# Verify transport capacity admission.
set -eux

echo "1..1"
set -v

set +e
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "progressive/0109_transport_capacity_admission"
result=$?
set -e
test "$result" -ne 77 || {
    echo "ok 1 - transport capacity admission # SKIP thread support disabled"
    exit 0
}
test "$result" -eq 0 || {
    echo "not ok 1 - transport capacity admission"
    exit 0
}
echo "ok 1 - transport capacity admission"
exit 0
