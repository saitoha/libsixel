#!/bin/sh
# Verify application strict keep.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "progressive/0054_application_strict_keep" || {
    echo "not ok 1 - application strict keep"
    exit 0
}
echo "ok 1 - application strict keep"
exit 0
