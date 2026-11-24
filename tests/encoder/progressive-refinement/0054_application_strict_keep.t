#!/bin/sh
# Verify application strict keep.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "progressive/0054_application_strict_keep" || {
    printf '\n%s\n' "not ok 1 - application strict keep"
    exit 0
}
printf '\n%s\n' "ok 1 - application strict keep"
exit 0
