#!/bin/sh
# Verify audit din99d.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "progressive/0039_audit_din99d" || {
    echo "not ok 1 - audit din99d"
    exit 0
}
echo "ok 1 - audit din99d"
exit 0
