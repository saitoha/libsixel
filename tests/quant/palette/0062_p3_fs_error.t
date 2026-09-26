#!/bin/sh
# Verify extended P3 FS error.
# Policy: docs/encoder/output/output-colorspace.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/0062_p3_fs_error_contract extended || {
    echo "not ok 1 - extended P3 FS error"
    exit 0
}

echo "ok 1 - extended P3 FS error"
exit 0
