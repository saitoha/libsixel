#!/bin/sh
# Verify application protected pixel.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "progressive/0056_application_protected_pixel" || {
    echo "not ok 1 - application protected pixel"
    exit 0
}
echo "ok 1 - application protected pixel"
exit 0
