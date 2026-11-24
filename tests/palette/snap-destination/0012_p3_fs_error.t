#!/bin/sh
# Verify extended P3 FS error.
# Policy: docs/encoder/output/output-colorspace.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/0061_p3_fs_error_contract extended || {
    printf '\n%s\n' "not ok 1 - extended P3 FS error"
    exit 0
}

printf '\n%s\n' "ok 1 - extended P3 FS error"
exit 0
