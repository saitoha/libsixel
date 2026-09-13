#!/bin/sh
# Verify queued transform lifetime.

set -eux

echo "1..1"
set -v

set +e
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "cms/0013_queued_transform_lifetime"
status=$?
set -e
test "$status" -ne 77 || {
    echo "ok 1 - queued transform lifetime # SKIP threads disabled"
    exit 0
}
test "$status" -eq 0 || {
    echo "not ok 1 - queued transform lifetime"
    exit 0
}

echo "ok 1 - queued transform lifetime"
exit 0
