#!/bin/sh
# Verify P3 FS nonfinite recovery.
# Policy: docs/functionality/output-colorspace.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/0062_p3_fs_error_contract finite || {
    echo "not ok 1 - P3 FS nonfinite recovery"
    exit 0
}

echo "ok 1 - P3 FS nonfinite recovery"
exit 0
