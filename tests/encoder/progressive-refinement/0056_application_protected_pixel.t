#!/bin/sh
# Verify application protected pixel.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "progressive/0056_application_protected_pixel" || {
    printf '\n%s\n' "not ok 1 - application protected pixel"
    exit 0
}
printf '\n%s\n' "ok 1 - application protected pixel"
exit 0
