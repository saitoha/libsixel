#!/bin/sh
# Policy: docs/encoder/output/delta-encoding.md
# Verify public admission snapshot.
set -eux

echo "1..1"
set -v

set +e
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "progressive/0100_public_admission_snapshot"
result=$?
set -e
test "$result" -ne 77 || {
    echo "ok 1 - public admission snapshot # SKIP thread support disabled"
    exit 0
}
test "$result" -eq 0 || {
    echo "not ok 1 - public admission snapshot"
    exit 0
}
echo "ok 1 - public admission snapshot"
exit 0
